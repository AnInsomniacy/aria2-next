#include "DefaultAuthResolver.h"
#include "prefs.h"
#include "AuthConfig.h"
#include "a2doctest.h"

namespace aria2 {

class DefaultAuthResolverTest {


private:
  std::unique_ptr<DefaultAuthResolver> resolver_;

public:
  void setUp()
  {
    //_netrc = new Netrc();
    //_option = new Option();
    resolver_.reset(new DefaultAuthResolver());
    //_factory->setOption(_option.get());
    resolver_->setDefaultCred("foo", "bar");
  }

  void testResolveAuthConfig_without_userDefined();
  void testResolveAuthConfig_with_userDefined();
};

A2_TEST(DefaultAuthResolverTest, testResolveAuthConfig_without_userDefined)
A2_TEST(DefaultAuthResolverTest, testResolveAuthConfig_with_userDefined)

void DefaultAuthResolverTest::testResolveAuthConfig_without_userDefined()
{
  auto authConfig = resolver_->resolveAuthConfig("localhost");
  REQUIRE_EQ(std::string("foo:bar"), authConfig->getAuthText());
}

void DefaultAuthResolverTest::testResolveAuthConfig_with_userDefined()
{
  resolver_->setUserDefinedCred("myname", "mypasswd");
  auto authConfig = resolver_->resolveAuthConfig("localhost");
  REQUIRE_EQ(std::string("myname:mypasswd"),
                       authConfig->getAuthText());
}

} // namespace aria2
