/* Copyright (C) 2026 aria2-next contributors. GPL-2.0-or-later. */
#pragma once

#include "core/Speed.h"
#include "ed2k/Store.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aria2::ed2k {

class Client;
class Network;

// Upload slots and the queue of clients waiting for one, scored as eMule
// does: waiting time multiplied by the credit the client earned by uploading
// to us. A slot is handed to the next client after about one part.
class UploadQueue {
public:
  explicit UploadQueue(Network& network);

  void setSlots(int slots) { slots_ = std::max(1, slots); }
  // Returns 0 when the client got a slot, else its rank in the queue.
  uint16_t request(const std::shared_ptr<Client>& client);
  uint16_t rank(const Client& client) const;
  // The client re-asked over UDP; it keeps its place.
  void touched(const Client& client);
  void remove(Client& client);
  void tick(Clock::time_point now);

  size_t uploading() const { return active_.size(); }
  size_t waiting() const { return waiting_.size(); }

  void uploaded(const std::string& userHash, uint64_t bytes);
  void downloaded(const std::string& userHash, uint64_t bytes);
  // eMule's credit modifier, from 1 to 10.
  double modifier(const std::string& userHash) const;
  std::vector<Credit> credits() const;
  void restore(const std::vector<Credit>& credits);

private:
  struct Waiting {
    std::shared_ptr<Client> client;
    Clock::time_point since;
    Clock::time_point lastAsked;
  };
  struct Active {
    std::shared_ptr<Client> client;
    Clock::time_point since;
  };

  double score(const Waiting& waiting, Clock::time_point now) const;
  void grant(Clock::time_point now);

  Network& network_;
  int slots_ = 3;
  std::vector<Waiting> waiting_;
  std::vector<Active> active_;
  std::map<std::string, Credit> credits_;
};

} // namespace aria2::ed2k
