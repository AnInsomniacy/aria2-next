/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#include "http/Client.h"

#include "base/Error.h"
#include "base/Log.h"
#include "core/Engine.h"

#include <boost/asio/generic/stream_protocol.hpp>
#include <boost/asio/post.hpp>

namespace aria2::http {

namespace asio = boost::asio;
using Protocol = asio::generic::stream_protocol;

// A socket watched for libcurl. Connection sockets are created and closed
// through libcurl's socket callbacks and owned here for their whole life, so
// a pooled connection keeps one registration. Other descriptors libcurl asks
// to watch (resolver wake-ups) are borrowed and released when unwatched.
struct Client::Socket {
  explicit Socket(asio::io_context& io) : socket(io) {}
  Protocol::socket socket;
  curl_socket_t fd = CURL_SOCKET_BAD;
  bool owned = false;
  int interest = CURL_POLL_NONE;
  bool reading = false;
  bool writing = false;
};

// ---------------------------------------------------------------- Transfer --

Transfer::Transfer(Client& client) : client_(client), easy_(curl_easy_init())
{
  if (!easy_) {
    throw Error(ErrorCode::Unknown, "curl_easy_init failed");
  }
  curl_easy_setopt(easy_, CURLOPT_PRIVATE, this);
  curl_easy_setopt(easy_, CURLOPT_ERRORBUFFER, errorBuffer_.data());
  curl_easy_setopt(easy_, CURLOPT_HEADERFUNCTION, headerCallback);
  curl_easy_setopt(easy_, CURLOPT_HEADERDATA, this);
  curl_easy_setopt(easy_, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(easy_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L);
}

Transfer::~Transfer()
{
  if (running_) {
    client_.remove(*this);
  }
  curl_easy_cleanup(easy_);
  curl_slist_free_all(headers_);
}

void Transfer::header(const std::string& line)
{
  if (auto* list = curl_slist_append(headers_, line.c_str())) {
    headers_ = list;
  }
}

void Transfer::start()
{
  if (headers_) {
    curl_easy_setopt(easy_, CURLOPT_HTTPHEADER, headers_);
  }
  client_.add(*this);
}

void Transfer::cancel()
{
  if (!running_ || cancelled_) {
    return;
  }
  cancelled_ = true;
  asio::post(client_.engine().io(), [self = shared_from_this()] {
    if (self->running_) {
      self->client_.remove(*self);
    }
  });
}

void Transfer::pause(bool paused)
{
  if (paused_ != paused && running_) {
    paused_ = paused;
    curl_easy_pause(easy_, paused ? CURLPAUSE_RECV : CURLPAUSE_CONT);
  }
}

long Transfer::responseCode() const
{
  long code = 0;
  curl_easy_getinfo(easy_, CURLINFO_RESPONSE_CODE, &code);
  return code;
}

std::string Transfer::effectiveUrl() const
{
  char* url = nullptr;
  curl_easy_getinfo(easy_, CURLINFO_EFFECTIVE_URL, &url);
  return url ? url : "";
}

size_t Transfer::headerCallback(char* data, size_t, size_t size, void* self)
{
  auto& transfer = *static_cast<Transfer*>(self);
  if (!transfer.cancelled_) {
    transfer.onHeader(std::string_view(data, size));
  }
  return size;
}

size_t Transfer::writeCallback(char* data, size_t, size_t size, void* self)
{
  auto& transfer = *static_cast<Transfer*>(self);
  if (transfer.cancelled_) {
    return size;
  }
  const auto consumed = transfer.onData(data, size);
  if (consumed == CURL_WRITEFUNC_PAUSE) {
    transfer.paused_ = true;
  }
  return consumed;
}

// ------------------------------------------------------------------ Client --

Client::Client(Engine& engine)
    : engine_(engine),
      io_(engine.io()),
      multi_(curl_multi_init()),
      share_(curl_share_init()),
      timer_(io_)
{
  curl_multi_setopt(multi_, CURLMOPT_SOCKETFUNCTION, socketCallback);
  curl_multi_setopt(multi_, CURLMOPT_SOCKETDATA, this);
  curl_multi_setopt(multi_, CURLMOPT_TIMERFUNCTION, timerCallback);
  curl_multi_setopt(multi_, CURLMOPT_TIMERDATA, this);
  curl_multi_setopt(multi_, CURLMOPT_MAX_HOST_CONNECTIONS, 0L);
  curl_multi_setopt(multi_, CURLMOPT_MAXCONNECTS, 256L);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  optionsChanged();
}

Client::~Client() { close(); }

void Client::optionsChanged()
{
  downloadLimit_.setRate(engine_.global().integer("max-overall-download-limit"));
}

void Client::close()
{
  if (!multi_) {
    return;
  }
  timer_.cancel();
  curl_multi_cleanup(multi_);
  multi_ = nullptr;
  curl_share_cleanup(share_);
  share_ = nullptr;
  for (auto& [fd, socket] : sockets_) {
    boost::system::error_code ignored;
    socket->socket.cancel(ignored);
    if (!socket->owned) {
      socket->socket.release(ignored);
    }
  }
  sockets_.clear();
}

void Client::add(Transfer& transfer)
{
  curl_easy_setopt(transfer.easy_, CURLOPT_OPENSOCKETFUNCTION, openCallback);
  curl_easy_setopt(transfer.easy_, CURLOPT_OPENSOCKETDATA, this);
  curl_easy_setopt(transfer.easy_, CURLOPT_CLOSESOCKETFUNCTION, closeCallback);
  curl_easy_setopt(transfer.easy_, CURLOPT_CLOSESOCKETDATA, this);
  if (curl_multi_add_handle(multi_, transfer.easy_) == CURLM_OK) {
    transfer.running_ = true;
    transfer.paused_ = false;
    transfer.cancelled_ = false;
  }
  else {
    throw Error(ErrorCode::Unknown, "curl_multi_add_handle failed");
  }
}

void Client::remove(Transfer& transfer)
{
  if (multi_) {
    curl_multi_remove_handle(multi_, transfer.easy_);
  }
  transfer.running_ = false;
}

curl_socket_t Client::openCallback(void* self, curlsocktype,
                                   curl_sockaddr* address)
{
  auto& client = *static_cast<Client*>(self);
  auto socket = std::make_shared<Socket>(client.io_);
  boost::system::error_code error;
  socket->socket.open(Protocol(address->family, address->protocol), error);
  if (error) {
    return CURL_SOCKET_BAD;
  }
  socket->fd = static_cast<curl_socket_t>(socket->socket.native_handle());
  socket->owned = true;
  client.sockets_[socket->fd] = socket;
  return socket->fd;
}

int Client::closeCallback(void* self, curl_socket_t fd)
{
  auto& client = *static_cast<Client*>(self);
  const auto found = client.sockets_.find(fd);
  if (found == client.sockets_.end()) {
#ifdef _WIN32
    return closesocket(fd);
#else
    return ::close(fd);
#endif
  }
  boost::system::error_code ignored;
  found->second->socket.close(ignored);
  client.sockets_.erase(found);
  return 0;
}

int Client::socketCallback(CURL*, curl_socket_t fd, int what, void* self,
                           void*)
{
  auto& client = *static_cast<Client*>(self);
  auto found = client.sockets_.find(fd);
  if (what == CURL_POLL_REMOVE) {
    if (found != client.sockets_.end()) {
      auto& socket = found->second;
      socket->interest = CURL_POLL_NONE;
      boost::system::error_code ignored;
      socket->socket.cancel(ignored);
      if (!socket->owned) {
        socket->socket.release(ignored);
        client.sockets_.erase(found);
      }
    }
    return 0;
  }
  if (found == client.sockets_.end()) {
    auto socket = std::make_shared<Socket>(client.io_);
    boost::system::error_code error;
    socket->socket.assign(Protocol(AF_INET, 0), fd, error);
    if (error) {
      log::error("Cannot watch descriptor {}: {}", fd, error.message());
      return -1;
    }
    socket->fd = fd;
    found = client.sockets_.emplace(fd, std::move(socket)).first;
  }
  found->second->interest = what;
  client.watch(found->second);
  return 0;
}

void Client::watch(const std::shared_ptr<Socket>& socket)
{
  const auto wait = [this, socket](Protocol::socket::wait_type type,
                                   bool& pending, int flag) {
    if (pending || !(socket->interest & flag)) {
      return;
    }
    pending = true;
    socket->socket.async_wait(
        type, [this, socket, &pending, flag](const boost::system::error_code& error) {
          pending = false;
          if (error || !multi_ || !(socket->interest & flag)) {
            return;
          }
          action(socket->fd,
                 flag == CURL_POLL_IN ? CURL_CSELECT_IN : CURL_CSELECT_OUT);
          // The socket may have been closed by the action.
          if (sockets_.contains(socket->fd) && sockets_[socket->fd] == socket) {
            watch(socket);
          }
        });
  };
  wait(Protocol::socket::wait_read, socket->reading, CURL_POLL_IN);
  wait(Protocol::socket::wait_write, socket->writing, CURL_POLL_OUT);
}

int Client::timerCallback(CURLM*, long timeoutMs, void* self)
{
  auto& client = *static_cast<Client*>(self);
  if (timeoutMs < 0) {
    client.timer_.cancel();
    return 0;
  }
  client.timer_.expires_after(std::chrono::milliseconds(timeoutMs));
  client.timer_.async_wait([&client](const boost::system::error_code& error) {
    if (!error && client.multi_) {
      client.action(CURL_SOCKET_TIMEOUT, 0);
    }
  });
  return 0;
}

void Client::action(curl_socket_t fd, int events)
{
  int running = 0;
  curl_multi_socket_action(multi_, fd, events, &running);
  finishTransfers();
}

void Client::finishTransfers()
{
  int queued = 0;
  while (auto* message = curl_multi_info_read(multi_, &queued)) {
    if (message->msg != CURLMSG_DONE) {
      continue;
    }
    Transfer* transfer = nullptr;
    curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &transfer);
    const auto result = message->data.result;
    curl_multi_remove_handle(multi_, message->easy_handle);
    transfer->running_ = false;
    if (!transfer->cancelled_) {
      // Hold the transfer: its owner may drop it from onDone().
      const auto keep = transfer->shared_from_this();
      transfer->onDone(result);
    }
  }
}

} // namespace aria2::http
