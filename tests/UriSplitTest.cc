#include "uri_split.h"

#include <cstring>
#include <string>

#include "a2doctest.h"

#include "uri_split.h"

namespace aria2 {

class UriSplitTest {


public:
  void testUriSplit();
  void testUriSplit_fail();
};

A2_TEST(UriSplitTest, testUriSplit)
A2_TEST(UriSplitTest, testUriSplit_fail)

namespace {
const char* fieldstr[] = {
    "USR_SCHEME",   "USR_HOST",     "USR_PORT", "USR_PATH",   "USR_QUERY",
    "USR_FRAGMENT", "USR_USERINFO", "USR_USER", "USR_PASSWD", "USR_BASENAME"};
} // namespace

#define CHECK_FIELD_SET(RES, FLAGS)                                            \
  for (int i = 0; i < USR_MAX; ++i) {                                          \
    int mask = 1 << i;                                                         \
    if ((FLAGS)&mask) {                                                        \
      REQUIRE_MESSAGE((RES.field_set & mask) != 0, fieldstr[i]);                \
    }                                                                          \
    else {                                                                     \
      REQUIRE_MESSAGE(!(RES.field_set & mask), fieldstr[i]);            \
    }                                                                          \
  }

namespace {
std::string mkstr(const uri_split_result& res, int field, const char* base)
{
  return std::string(base + res.fields[field].off, res.fields[field].len);
}
} // namespace

void UriSplitTest::testUriSplit()
{
  uri_split_result res;
  const char* uri;
  uri = "http://aria2.sf.net/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH));
  REQUIRE_EQ(std::string("http"), mkstr(res, USR_SCHEME, uri));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));

  uri = "http://user@aria2.sf.net/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_USERINFO) | (1 << USR_USER));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));

  uri = "http://user:pass@aria2.sf.net/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_USERINFO) | (1 << USR_USER) |
                           (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("user:pass"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));

  // According to RFC 3986, @ in userinfo is illegal. But many people
  // have e-mail account as username and don't understand
  // percent-encoding and keep getting errors putting it in URI in
  // unecoded form. Because of this, we support @ in username.
  uri = "http://user@foo.com:pass@aria2.sf.net/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_USERINFO) | (1 << USR_USER) |
                           (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("user@foo.com:pass"),
                       mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user@foo.com"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));

  // Port processed in URI_MAYBE_USER -> URI_PORT
  uri = "https://aria2.sf.net:443/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_PATH));
  REQUIRE_EQ(std::string("https"), mkstr(res, USR_SCHEME, uri));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // Port processed in URI_PORT
  uri = "https://user:pass@aria2.sf.net:443/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_PATH) | (1 << USR_USERINFO) |
                           (1 << USR_USER) | (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("user:pass"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // Port processed in URI_MAYBE_PASSWD
  uri = "https://user@aria2.sf.net:443/path/";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_PATH) | (1 << USR_USERINFO) |
                           (1 << USR_USER));
  REQUIRE_EQ(std::string("aria2.sf.net"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/path/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // Port processed in URI_MAYBE_PASSWD
  uri = "http://aria2";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));

  uri = "http://aria2:8080";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ((uint16_t)8080, res.port);

  uri = "http://user@aria2";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) |
                           (1 << USR_USERINFO) | (1 << USR_USER));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));

  uri = "http://user:@aria2";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) |
                           (1 << USR_USERINFO) | (1 << USR_USER) |
                           (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user:"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string(""), mkstr(res, USR_PASSWD, uri));

  uri = "http://aria2/?foo#bar";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_QUERY) | (1 << USR_FRAGMENT));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("foo"), mkstr(res, USR_QUERY, uri));
  REQUIRE_EQ(std::string("bar"), mkstr(res, USR_FRAGMENT, uri));

  // URI_MAYBE_USER
  uri = "http://aria2?foo";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_QUERY));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("foo"), mkstr(res, USR_QUERY, uri));

  // URI_MAYBE_USER
  uri = "http://aria2#bar";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res,
                  (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_FRAGMENT));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("bar"), mkstr(res, USR_FRAGMENT, uri));

  // URI_MAYBE_PASSWD
  uri = "https://aria2:443?foo";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_QUERY));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("foo"), mkstr(res, USR_QUERY, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // URI_MAYBE_PASSWD
  uri = "https://aria2:443#bar";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_FRAGMENT));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("bar"), mkstr(res, USR_FRAGMENT, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // URI_PORT
  uri = "https://user:pass@aria2:443?foo";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_QUERY) | (1 << USR_USERINFO) |
                           (1 << USR_USER) | (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));
  REQUIRE_EQ(std::string("foo"), mkstr(res, USR_QUERY, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // URI_PORT
  uri = "https://user:pass@aria2:443#bar";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_FRAGMENT) | (1 << USR_USERINFO) |
                           (1 << USR_USER) | (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));
  REQUIRE_EQ(std::string("bar"), mkstr(res, USR_FRAGMENT, uri));
  REQUIRE_EQ((uint16_t)443, res.port);

  // URI_HOST
  uri = "http://user:pass@aria2?foo";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_QUERY) |
                           (1 << USR_USERINFO) | (1 << USR_USER) |
                           (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));
  REQUIRE_EQ(std::string("foo"), mkstr(res, USR_QUERY, uri));

  // URI_HOST
  uri = "http://user:pass@aria2#bar";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) |
                           (1 << USR_FRAGMENT) | (1 << USR_USERINFO) |
                           (1 << USR_USER) | (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));
  REQUIRE_EQ(std::string("bar"), mkstr(res, USR_FRAGMENT, uri));

  // empty query
  uri = "http://aria2/?";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_QUERY));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string(""), mkstr(res, USR_QUERY, uri));

  // empty fragment
  uri = "http://aria2/#";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_FRAGMENT));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string(""), mkstr(res, USR_FRAGMENT, uri));

  // empty query and fragment
  uri = "http://aria2/?#";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_QUERY) | (1 << USR_FRAGMENT));
  REQUIRE_EQ(std::string("aria2"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string(""), mkstr(res, USR_QUERY, uri));
  REQUIRE_EQ(std::string(""), mkstr(res, USR_FRAGMENT, uri));

  // IPv6 numeric address
  uri = "http://[::1]";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST));
  REQUIRE_EQ(std::string("::1"), mkstr(res, USR_HOST, uri));
  REQUIRE((res.flags & USF_IPV6ADDR) != 0);

  uri = "https://[::1]:443";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT));
  REQUIRE_EQ(std::string("::1"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ((uint16_t)443, res.port);
  REQUIRE((res.flags & USF_IPV6ADDR) != 0);

  // USR_MAYBE_USER
  uri = "https://user@[::1]";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) |
                           (1 << USR_USERINFO) | (1 << USR_USER));
  REQUIRE_EQ(std::string("::1"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE((res.flags & USF_IPV6ADDR) != 0);

  // USR_BEFORE_HOST
  uri = "https://user:pass@[::1]";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) |
                           (1 << USR_USERINFO) | (1 << USR_USER) |
                           (1 << USR_PASSWD));
  REQUIRE_EQ(std::string("::1"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user:pass"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("user"), mkstr(res, USR_USER, uri));
  REQUIRE_EQ(std::string("pass"), mkstr(res, USR_PASSWD, uri));
  REQUIRE((res.flags & USF_IPV6ADDR) != 0);

  uri = "http://aria2/f";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_BASENAME));
  REQUIRE_EQ(std::string("/f"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("f"), mkstr(res, USR_BASENAME, uri));

  uri = "http://[::1]/f";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_BASENAME));
  REQUIRE_EQ(std::string("::1"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("/f"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("f"), mkstr(res, USR_BASENAME, uri));

  uri = "http://[::1]:8080/f";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PORT) |
                           (1 << USR_PATH) | (1 << USR_BASENAME));
  REQUIRE_EQ((uint16_t)8080, res.port);
  REQUIRE_EQ(std::string("/f"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("f"), mkstr(res, USR_BASENAME, uri));

  uri = "https://user:pass@host/f";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) |
                           (1 << USR_USERINFO) | (1 << USR_USER) |
                           (1 << USR_PASSWD) | (1 << USR_PATH) |
                           (1 << USR_BASENAME));
  REQUIRE_EQ(std::string("host"), mkstr(res, USR_HOST, uri));
  REQUIRE_EQ(std::string("user:pass"), mkstr(res, USR_USERINFO, uri));
  REQUIRE_EQ(std::string("/f"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("f"), mkstr(res, USR_BASENAME, uri));

  uri = "http://aria2/index.html?foo";
  memset(&res, 0, sizeof(res));
  REQUIRE_EQ(0, uri_split(&res, uri));
  CHECK_FIELD_SET(res, (1 << USR_SCHEME) | (1 << USR_HOST) | (1 << USR_PATH) |
                           (1 << USR_QUERY) | (1 << USR_BASENAME));
  REQUIRE_EQ(std::string("/index.html"), mkstr(res, USR_PATH, uri));
  REQUIRE_EQ(std::string("index.html"),
                       mkstr(res, USR_BASENAME, uri));
  REQUIRE_EQ(std::string("foo"), mkstr(res, USR_QUERY, uri));
}

void UriSplitTest::testUriSplit_fail()
{
  REQUIRE_EQ(-1, uri_split(nullptr, ""));
  REQUIRE_EQ(-1, uri_split(nullptr, "h"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http:"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http:a"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http:/"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http:/a"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://:host"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://@user@host"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:pass"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:65536"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:pass?"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:pass@host:65536"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:pass@host:x"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:pass@host:80x"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user@"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://[]"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://[::"));
  REQUIRE_EQ(
      -1, uri_split(nullptr, "https://U@[7:7A:EC6f:::4:Cc6:dDe:75]"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user[::1]"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user[::1]x"));
  REQUIRE_EQ(-1, uri_split(nullptr, "http://user:pass[::1]"));
}

} // namespace aria2
