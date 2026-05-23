#include "RpcResponse.h"

#include <cppunit/extensions/HelperMacros.h>

namespace aria2 {

namespace rpc {

class RpcResponseTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(RpcResponseTest);
  CPPUNIT_TEST(testToJson);
  CPPUNIT_TEST_SUITE_END();

public:
  void testToJson();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RpcResponseTest);

void RpcResponseTest::testToJson()
{
  std::vector<RpcResponse> results;
  {
    auto param = List::g();
    param->append(Integer::g(1));
    RpcResponse res(0, RpcResponse::AUTHORIZED, std::move(param),
                    String::g("9"));
    results.push_back(std::move(res));
    std::string s = toJson(results.back(), "", false);
    CPPUNIT_ASSERT_EQUAL(std::string("{\"id\":\"9\","
                                     "\"jsonrpc\":\"2.0\","
                                     "\"result\":[1]}"),
                         s);
    // with callback
    s = toJson(results.back(), "cb", false);
    CPPUNIT_ASSERT_EQUAL(std::string("cb({\"id\":\"9\","
                                     "\"jsonrpc\":\"2.0\","
                                     "\"result\":[1]})"),
                         s);
  }
  {
    // error response
    auto param = Dict::g();
    param->put("code", Integer::g(1));
    param->put("message", "HELLO ERROR");
    RpcResponse res(1, RpcResponse::AUTHORIZED, std::move(param), Null::g());
    results.push_back(std::move(res));
    std::string s = toJson(results.back(), "", false);
    CPPUNIT_ASSERT_EQUAL(std::string("{\"id\":null,"
                                     "\"jsonrpc\":\"2.0\","
                                     "\"error\":{\"code\":1,"
                                     "\"message\":\"HELLO ERROR\"}"
                                     "}"),
                         s);
    // with callback
    s = toJson(results.back(), "cb", false);
    CPPUNIT_ASSERT_EQUAL(std::string("cb({\"id\":null,"
                                     "\"jsonrpc\":\"2.0\","
                                     "\"error\":{\"code\":1,"
                                     "\"message\":\"HELLO ERROR\"}"
                                     "})"),
                         s);
  }
  {
    // batch response
    std::string s = toJsonBatch(results, "", false);
    CPPUNIT_ASSERT_EQUAL(std::string("["
                                     "{\"id\":\"9\","
                                     "\"jsonrpc\":\"2.0\","
                                     "\"result\":[1]},"
                                     "{\"id\":null,"
                                     "\"jsonrpc\":\"2.0\","
                                     "\"error\":{\"code\":1,"
                                     "\"message\":\"HELLO ERROR\"}"
                                     "}"
                                     "]"),
                         s);
    // with callback
    s = toJsonBatch(results, "cb", false);
    CPPUNIT_ASSERT_EQUAL(std::string("cb(["
                                     "{\"id\":\"9\","
                                     "\"jsonrpc\":\"2.0\","
                                     "\"result\":[1]},"
                                     "{\"id\":null,"
                                     "\"jsonrpc\":\"2.0\","
                                     "\"error\":{\"code\":1,"
                                     "\"message\":\"HELLO ERROR\"}"
                                     "}"
                                     "])"),
                         s);
  }
}

} // namespace rpc

} // namespace aria2
