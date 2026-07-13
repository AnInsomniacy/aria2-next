#include "FtpConnection.h"

#include <iostream>
#include <cstring>

#include "a2doctest.h"

#include "Exception.h"
#include "util.h"
#include "SocketCore.h"
#include "Request.h"
#include "Option.h"
#include "DlRetryEx.h"
#include "DlAbortEx.h"
#include "AuthConfigFactory.h"
#include "AuthConfig.h"

namespace aria2 {

class FtpConnectionTest {


private:
  std::shared_ptr<SocketCore> serverSocket_;
  uint16_t listenPort_;
  std::shared_ptr<SocketCore> clientSocket_;
  std::shared_ptr<FtpConnection> ftp_;
  std::shared_ptr<Option> option_;
  std::shared_ptr<AuthConfigFactory> authConfigFactory_;
  std::shared_ptr<Request> req_;

public:
  void setUp()
  {
    option_.reset(new Option());
    authConfigFactory_.reset(new AuthConfigFactory());

    //_ftpServerSocket.reset(new SocketCore());
    std::shared_ptr<SocketCore> listenSocket(new SocketCore());
    listenSocket->bind(0);
    listenSocket->beginListen();
    listenSocket->setBlockingMode();
    listenPort_ = listenSocket->getAddrInfo().port;

    req_.reset(new Request());
    req_->setUri("ftp://localhost/dir%20sp/hello%20world.img");

    clientSocket_.reset(new SocketCore());
    clientSocket_->establishConnection("localhost", listenPort_);

    while (!clientSocket_->isWritable(0))
      ;

    serverSocket_ = listenSocket->acceptConnection();
    serverSocket_->setBlockingMode();
    ftp_.reset(new FtpConnection(
        1, clientSocket_, req_,
        authConfigFactory_->createAuthConfig(req_, option_.get()),
        option_.get()));
  }

  void tearDown() {}

  void testSendMdtm();
  void testReceiveMdtmResponse();
  void testReceiveResponse();
  void testReceiveResponse_overflow();
  void testSendPwd();
  void testReceivePwdResponse();
  void testReceivePwdResponse_unquotedResponse();
  void testReceivePwdResponse_badStatus();
  void testSendCwd();
  void testSendSize();
  void testReceiveSizeResponse();
  void testSendRetr();
  void testReceiveEpsvResponse();
};

A2_TEST(FtpConnectionTest, testReceiveResponse)
A2_TEST(FtpConnectionTest, testReceiveResponse_overflow)
A2_TEST(FtpConnectionTest, testSendMdtm)
A2_TEST(FtpConnectionTest, testReceiveMdtmResponse)
A2_TEST(FtpConnectionTest, testSendPwd)
A2_TEST(FtpConnectionTest, testReceivePwdResponse)
A2_TEST(FtpConnectionTest, testReceivePwdResponse_unquotedResponse)
A2_TEST(FtpConnectionTest, testReceivePwdResponse_badStatus)
A2_TEST(FtpConnectionTest, testSendCwd)
A2_TEST(FtpConnectionTest, testSendSize)
A2_TEST(FtpConnectionTest, testReceiveSizeResponse)
A2_TEST(FtpConnectionTest, testSendRetr)
A2_TEST(FtpConnectionTest, testReceiveEpsvResponse)

namespace {
void waitRead(const std::shared_ptr<SocketCore>& socket)
{
  while (!socket->isReadable(0))
    ;
}
} // namespace

void FtpConnectionTest::testReceiveResponse()
{
  serverSocket_->writeData("100");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receiveResponse());
  serverSocket_->writeData(" single line response");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receiveResponse());
  serverSocket_->writeData("\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(100, ftp_->receiveResponse());
  // 2 responses in the buffer
  serverSocket_->writeData("101 single1\r\n"
                           "102 single2\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(101, ftp_->receiveResponse());
  REQUIRE_EQ(102, ftp_->receiveResponse());

  serverSocket_->writeData("103-multi line response\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receiveResponse());
  serverSocket_->writeData("103-line2\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receiveResponse());
  serverSocket_->writeData("103");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receiveResponse());
  serverSocket_->writeData(" ");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receiveResponse());
  serverSocket_->writeData("last\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(103, ftp_->receiveResponse());

  serverSocket_->writeData("104-multi\r\n"
                           "104 \r\n"
                           "105-multi\r\n"
                           "105 \r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(104, ftp_->receiveResponse());
  REQUIRE_EQ(105, ftp_->receiveResponse());
}

void FtpConnectionTest::testSendMdtm()
{
  ftp_->sendMdtm();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  data[len] = '\0';
  REQUIRE_EQ(std::string("MDTM hello world.img\r\n"),
                       std::string(data));
}

void FtpConnectionTest::testReceiveMdtmResponse()
{
  {
    Time t;
    serverSocket_->writeData("213 20080908124312");
    waitRead(clientSocket_);
    REQUIRE_EQ(0, ftp_->receiveMdtmResponse(t));
    serverSocket_->writeData("\r\n");
    waitRead(clientSocket_);
    REQUIRE_EQ(213, ftp_->receiveMdtmResponse(t));
    REQUIRE_EQ((time_t)1220877792, t.getTimeFromEpoch());
  }
  {
    // see milli second part is ignored
    Time t;
    serverSocket_->writeData("213 20080908124312.014\r\n");
    waitRead(clientSocket_);
    REQUIRE_EQ(213, ftp_->receiveMdtmResponse(t));
    REQUIRE_EQ((time_t)1220877792, t.getTimeFromEpoch());
  }
  {
    // hhmmss part is missing
    Time t;
    serverSocket_->writeData("213 20080908\r\n");
    waitRead(clientSocket_);
    REQUIRE_EQ(213, ftp_->receiveMdtmResponse(t));
    REQUIRE(t.bad());
  }
  {
    // invalid month: 19
    Time t;
    serverSocket_->writeData("213 20081908124312\r\n");
    waitRead(clientSocket_);
    REQUIRE_EQ(213, ftp_->receiveMdtmResponse(t));
#ifdef HAVE_TIMEGM
    // Time will be normalized. Wed Jul 8 12:43:12 2009
    REQUIRE_EQ((time_t)1247056992, t.getTimeFromEpoch());
#else  // !HAVE_TIMEGM
    // The replacement timegm does not normalize.
    REQUIRE_EQ((time_t)-1, t.getTimeFromEpoch());
#endif // !HAVE_TIMEGM
  }
  {
    Time t;
    serverSocket_->writeData("550 File Not Found\r\n");
    waitRead(clientSocket_);
    REQUIRE_EQ(550, ftp_->receiveMdtmResponse(t));
  }
}

void FtpConnectionTest::testReceiveResponse_overflow()
{
  char data[1_k];
  memset(data, 0, sizeof(data));
  memcpy(data, "213 ", 4);
  for (int i = 0; i < 64; ++i) {
    serverSocket_->writeData(data, sizeof(data));
    waitRead(clientSocket_);
    REQUIRE_EQ(0, ftp_->receiveResponse());
  }
  serverSocket_->writeData(data, sizeof(data));
  waitRead(clientSocket_);
  try {
    ftp_->receiveResponse();
    FAIL("exception must be thrown.");
  }
  catch (DlRetryEx& e) {
    // success
  }
}

void FtpConnectionTest::testSendPwd()
{
  ftp_->sendPwd();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  REQUIRE_EQ((size_t)5, len);
  data[len] = '\0';
  REQUIRE_EQ(std::string("PWD\r\n"), std::string(data));
}

void FtpConnectionTest::testReceivePwdResponse()
{
  std::string pwd;
  serverSocket_->writeData("257 ");
  waitRead(clientSocket_);
  REQUIRE_EQ(0, ftp_->receivePwdResponse(pwd));
  REQUIRE(pwd.empty());
  serverSocket_->writeData("\"/dir/to\" is your directory.\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(257, ftp_->receivePwdResponse(pwd));
  REQUIRE_EQ(std::string("/dir/to"), pwd);
}

void FtpConnectionTest::testReceivePwdResponse_unquotedResponse()
{
  std::string pwd;
  serverSocket_->writeData("257 /dir/to\r\n");
  waitRead(clientSocket_);
  try {
    ftp_->receivePwdResponse(pwd);
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

void FtpConnectionTest::testReceivePwdResponse_badStatus()
{
  std::string pwd;
  serverSocket_->writeData("500 failed\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(500, ftp_->receivePwdResponse(pwd));
  REQUIRE(pwd.empty());
}

void FtpConnectionTest::testSendCwd()
{
  ftp_->sendCwd("%2Fdir%20sp");
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  data[len] = '\0';
  REQUIRE_EQ(std::string("CWD /dir sp\r\n"), std::string(data));
}

void FtpConnectionTest::testSendSize()
{
  ftp_->sendSize();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  REQUIRE_EQ(std::string("SIZE hello world.img\r\n"),
                       std::string(&data[0], &data[len]));
}

void FtpConnectionTest::testReceiveSizeResponse()
{
  serverSocket_->writeData("213 4294967296\r\n");
  waitRead(clientSocket_);
  int64_t size;
  REQUIRE_EQ(213, ftp_->receiveSizeResponse(size));
  REQUIRE_EQ((int64_t)4294967296LL, size);
}

void FtpConnectionTest::testSendRetr()
{
  ftp_->sendRetr();
  char data[32];
  size_t len = sizeof(data);
  serverSocket_->readData(data, len);
  REQUIRE_EQ(std::string("RETR hello world.img\r\n"),
                       std::string(&data[0], &data[len]));
}

void FtpConnectionTest::testReceiveEpsvResponse()
{
  serverSocket_->writeData("229 Success (|||12000|)\r\n");
  waitRead(clientSocket_);
  uint16_t port = 0;
  REQUIRE_EQ(229, ftp_->receiveEpsvResponse(port));
  REQUIRE_EQ((uint16_t)12000, port);

  serverSocket_->writeData("229 Success |||12000|)\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(229, ftp_->receiveEpsvResponse(port));
  REQUIRE_EQ((uint16_t)0, port);

  serverSocket_->writeData("229 Success (|||12000|\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(229, ftp_->receiveEpsvResponse(port));
  REQUIRE_EQ((uint16_t)0, port);

  serverSocket_->writeData("229 Success ()|||12000|\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(229, ftp_->receiveEpsvResponse(port));
  REQUIRE_EQ((uint16_t)0, port);

  serverSocket_->writeData("229 Success )(|||12000|)\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(229, ftp_->receiveEpsvResponse(port));
  REQUIRE_EQ((uint16_t)0, port);

  serverSocket_->writeData("229 Success )(||12000|)\r\n");
  waitRead(clientSocket_);
  REQUIRE_EQ(229, ftp_->receiveEpsvResponse(port));
  REQUIRE_EQ((uint16_t)0, port);
}

} // namespace aria2
