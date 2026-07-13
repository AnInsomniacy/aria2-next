#include "UTMetadataRejectExtensionMessage.h"

#include <iostream>

#include "a2doctest.h"

#include "BtConstants.h"
#include "DlAbortEx.h"

namespace aria2 {

class UTMetadataRejectExtensionMessageTest {


public:
  void testGetExtensionMessageID();
  void testGetBencodedReject();
  void testToString();
  void testDoReceivedAction();
};

A2_TEST(UTMetadataRejectExtensionMessageTest, testGetExtensionMessageID)
A2_TEST(UTMetadataRejectExtensionMessageTest, testGetBencodedReject)
A2_TEST(UTMetadataRejectExtensionMessageTest, testToString)
A2_TEST(UTMetadataRejectExtensionMessageTest, testDoReceivedAction)

void UTMetadataRejectExtensionMessageTest::testGetExtensionMessageID()
{
  UTMetadataRejectExtensionMessage msg(1);
  REQUIRE_EQ((uint8_t)1, msg.getExtensionMessageID());
}

void UTMetadataRejectExtensionMessageTest::testGetBencodedReject()
{
  UTMetadataRejectExtensionMessage msg(1);
  msg.setIndex(1);
  REQUIRE_EQ(std::string("d8:msg_typei2e5:piecei1ee"),
                       msg.getPayload());
}

void UTMetadataRejectExtensionMessageTest::testToString()
{
  UTMetadataRejectExtensionMessage msg(1);
  msg.setIndex(100);
  REQUIRE_EQ(std::string("ut_metadata reject piece=100"),
                       msg.toString());
}

void UTMetadataRejectExtensionMessageTest::testDoReceivedAction()
{
  UTMetadataRejectExtensionMessage msg(1);
  msg.setIndex(0);
  try {
    msg.doReceivedAction();
    FAIL("exception must be thrown.");
  }
  catch (DlAbortEx& e) {
    // success
  }
}

} // namespace aria2
