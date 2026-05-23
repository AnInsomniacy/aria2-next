#include "rpc_helper.h"

#include <cppunit/extensions/HelperMacros.h>

#include "RpcRequest.h"
#include "RecoverableException.h"

namespace aria2 {

namespace rpc {

class RpcHelperTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(RpcHelperTest);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}

  void tearDown() {}
};

CPPUNIT_TEST_SUITE_REGISTRATION(RpcHelperTest);

} // namespace rpc

} // namespace aria2
