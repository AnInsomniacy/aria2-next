#include "rpc_helper.h"

#include "a2doctest.h"

#include "RpcRequest.h"
#include "RecoverableException.h"
#ifdef ENABLE_XML_RPC
#  include "XmlRpcRequestParserStateMachine.h"
#endif // ENABLE_XML_RPC

namespace aria2 {

namespace rpc {

class RpcHelperTest {


public:
  void setUp() {}

  void tearDown() {}

#ifdef ENABLE_XML_RPC
  void testParseMemory();
  void testParseMemory_shouldFail();
  void testParseMemory_withoutParams();
  void testParseMemory_withoutStringTag();
#endif // ENABLE_XML_RPC
};

#ifdef ENABLE_XML_RPC
A2_TEST(RpcHelperTest, testParseMemory)
A2_TEST(RpcHelperTest, testParseMemory_shouldFail)
A2_TEST(RpcHelperTest, testParseMemory_withoutStringTag)
#endif // ENABLE_XML_RPC

#ifdef ENABLE_XML_RPC

void RpcHelperTest::testParseMemory()
{
  std::string s =
      "<?xml version=\"1.0\"?>"
      "<methodCall>"
      "  <methodName>aria2.addURI</methodName>"
      "    <params>"
      "      <param>"
      "        <value><i4>100</i4></value>"
      "      </param>"
      "      <param>"
      "       <value>"
      "         <struct>"
      "           <member>"
      "             <name>max-count</name>"
      "             <value><i4>65535</i4></value>"
      "           </member>"
      "           <member>"
      "             <name>seed-ratio</name>"
      "             <value><double>0.99</double></value>"
      "           </member>"
      "         </struct>"
      "       </value>"
      "     </param>"
      "     <param>"
      "       <value>"
      "         <array>"
      "           <data>"
      "             <value><string>pudding</string></value>"
      "             <value><base64>aGVsbG8gd29ybGQ=</base64></value>"
      "           </data>"
      "         </array>"
      "       </value>"
      "     </param>"
      "   </params>"
      "</methodCall>";
  RpcRequest req = xmlParseMemory(s.c_str(), s.size());

  REQUIRE_EQ(std::string("aria2.addURI"), req.methodName);
  REQUIRE_EQ((size_t)3, req.params->size());
  REQUIRE_EQ((Integer::ValueType)100,
                       downcast<Integer>(req.params->get(0))->i());
  const Dict* dict = downcast<Dict>(req.params->get(1));
  REQUIRE_EQ((Integer::ValueType)65535,
                       downcast<Integer>(dict->get("max-count"))->i());
  // Current implementation handles double as string.
  REQUIRE_EQ(std::string("0.99"),
                       downcast<String>(dict->get("seed-ratio"))->s());
  const List* list = downcast<List>(req.params->get(2));
  REQUIRE_EQ(std::string("pudding"),
                       downcast<String>(list->get(0))->s());
  REQUIRE_EQ(std::string("hello world"),
                       downcast<String>(list->get(1))->s());
}

void RpcHelperTest::testParseMemory_shouldFail()
{
  try {
    std::string s = "<methodCall>"
                    "  <methodName>aria2.addURI</methodName>"
                    "    <params>"
                    "      <param>"
                    "        <value><i4>100</i4></value>"
                    "      </param>";
    xmlParseMemory(s.c_str(), s.size());
    FAIL("exception must be thrown.");
  }
  catch (RecoverableException& e) {
    // success
  }
}

void RpcHelperTest::testParseMemory_withoutParams()
{
  {
    std::string s = "<methodCall>"
                    "  <methodName>aria2.addURI</methodName>"
                    "    <params>"
                    "    </params>"
                    "</methodCall>";
    RpcRequest req = xmlParseMemory(s.c_str(), s.size());
    REQUIRE(req.params);
  }
  {
    std::string s = "<methodCall>"
                    "  <methodName>aria2.addURI</methodName>"
                    "</methodCall>";
    RpcRequest req = xmlParseMemory(s.c_str(), s.size());
    REQUIRE(req.params->size());
  }
}

void RpcHelperTest::testParseMemory_withoutStringTag()
{
  std::string s = "<?xml version=\"1.0\"?>"
                  "<methodCall>"
                  "  <methodName>aria2.addUri</methodName>"
                  "    <params>"
                  "      <param>"
                  "        <value>http://aria2.sourceforge.net</value>"
                  "      </param>"
                  "      <param>"
                  "        <value>http://aria2.<foo/>sourceforge.net</value>"
                  "      </param>"
                  "      <param>"
                  "       <value>"
                  "         <struct>"
                  "           <member>"
                  "             <name>hello</name>"
                  "             <value>world</value>"
                  "           </member>"
                  "         </struct>"
                  "       </value>"
                  "     </param>"
                  "     <param>"
                  "       <value>"
                  "         <array>"
                  "           <data>"
                  "             <value>apple</value>"
                  "             <value>banana</value>"
                  "             <value><string>lemon</string>peanuts</value>"
                  "           </data>"
                  "         </array>"
                  "       </value>"
                  "     </param>"
                  "   </params>"
                  "</methodCall>";
  RpcRequest req = xmlParseMemory(s.c_str(), s.size());

  REQUIRE_EQ((size_t)4, req.params->size());
  REQUIRE_EQ(std::string("http://aria2.sourceforge.net"),
                       downcast<String>(req.params->get(0))->s());
  REQUIRE_EQ(std::string("http://aria2.sourceforge.net"),
                       downcast<String>(req.params->get(1))->s());
  const Dict* dict = downcast<Dict>(req.params->get(2));
  REQUIRE_EQ(std::string("world"),
                       downcast<String>(dict->get("hello"))->s());
  const List* list = downcast<List>(req.params->get(3));
  REQUIRE_EQ(std::string("apple"),
                       downcast<String>(list->get(0))->s());
  REQUIRE_EQ(std::string("banana"),
                       downcast<String>(list->get(1))->s());
  REQUIRE_EQ(std::string("lemon"),
                       downcast<String>(list->get(2))->s());
}

#endif // ENABLE_XML_RPC

} // namespace rpc

} // namespace aria2
