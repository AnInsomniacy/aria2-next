#include "uri.h"

#include "a2doctest.h"

namespace aria2 {

namespace uri {

class UriTest {


public:
  void testSetUri1();
  void testSetUri2();
  void testSetUri3();
  void testSetUri4();
  void testSetUri5();
  void testSetUri6();
  void testSetUri7();
  void testSetUri8();
  void testSetUri9();
  void testSetUri10();
  void testSetUri11();
  void testSetUri12();
  void testSetUri13();
  void testSetUri14();
  void testSetUri15();
  void testSetUri16();
  void testSetUri18();
  void testSetUri19();
  void testSetUri20();
  void testSetUri_username();
  void testSetUri_usernamePassword();
  void testSetUri_zeroUsername();
  void testSetUri_ipv6();
  void testInnerLink();
  void testConstruct();
  void testSwap();
  void testJoinUri();
  void testJoinPath();
};

A2_TEST(UriTest, testSetUri1)
A2_TEST(UriTest, testSetUri2)
A2_TEST(UriTest, testSetUri3)
A2_TEST(UriTest, testSetUri4)
A2_TEST(UriTest, testSetUri5)
A2_TEST(UriTest, testSetUri6)
A2_TEST(UriTest, testSetUri7)
A2_TEST(UriTest, testSetUri8)
A2_TEST(UriTest, testSetUri9)
A2_TEST(UriTest, testSetUri10)
A2_TEST(UriTest, testSetUri11)
A2_TEST(UriTest, testSetUri12)
A2_TEST(UriTest, testSetUri13)
A2_TEST(UriTest, testSetUri14)
A2_TEST(UriTest, testSetUri15)
A2_TEST(UriTest, testSetUri16)
A2_TEST(UriTest, testSetUri18)
A2_TEST(UriTest, testSetUri19)
A2_TEST(UriTest, testSetUri20)
A2_TEST(UriTest, testSetUri_username)
A2_TEST(UriTest, testSetUri_usernamePassword)
A2_TEST(UriTest, testSetUri_zeroUsername)
A2_TEST(UriTest, testSetUri_ipv6)
A2_TEST(UriTest, testInnerLink)
A2_TEST(UriTest, testConstruct)
A2_TEST(UriTest, testSwap)
A2_TEST(UriTest, testJoinUri)
A2_TEST(UriTest, testJoinPath)

void UriTest::testSetUri1()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com/");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ((uint16_t)80, us.port);
  REQUIRE_EQ(std::string("aria.rednoah.com"), us.host);
  REQUIRE_EQ(std::string("/"), us.dir);
  REQUIRE_EQ(std::string(""), us.file);
  REQUIRE_EQ(std::string(""), us.query);
  REQUIRE_EQ(std::string(""), us.username);
  REQUIRE_EQ(std::string(""), us.password);
  REQUIRE(!us.ipv6LiteralAddress);
}

void UriTest::testSetUri2()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com:8080/index.html");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ((uint16_t)8080, us.port);
  REQUIRE_EQ(std::string("aria.rednoah.com"), us.host);
  REQUIRE_EQ(std::string("/"), us.dir);
  REQUIRE_EQ(std::string("index.html"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri3()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com/aria2/index.html");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ((uint16_t)80, us.port);
  REQUIRE_EQ(std::string("aria.rednoah.com"), us.host);
  REQUIRE_EQ(std::string("/aria2/"), us.dir);
  REQUIRE_EQ(std::string("index.html"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri4()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com/aria2/aria3/index.html");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ((uint16_t)80, us.port);
  REQUIRE_EQ(std::string("aria.rednoah.com"), us.host);
  REQUIRE_EQ(std::string("/aria2/aria3/"), us.dir);
  REQUIRE_EQ(std::string("index.html"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri5()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com/aria2/aria3/");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ((uint16_t)80, us.port);
  REQUIRE_EQ(std::string("aria.rednoah.com"), us.host);
  REQUIRE_EQ(std::string("/aria2/aria3/"), us.dir);
  REQUIRE_EQ(std::string(""), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri6()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com/aria2/aria3");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ((uint16_t)80, us.port);
  REQUIRE_EQ(std::string("aria.rednoah.com"), us.host);
  REQUIRE_EQ(std::string("/aria2/"), us.dir);
  REQUIRE_EQ(std::string("aria3"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri7()
{
  UriStruct us;
  bool v = parse(us, "http://");

  REQUIRE(!v);
}

void UriTest::testSetUri8()
{
  UriStruct us;
  bool v = parse(us, "http:/aria.rednoah.com");

  REQUIRE(!v);
}

void UriTest::testSetUri9()
{
  UriStruct us;
  bool v = parse(us, "h");

  REQUIRE(!v);
}

void UriTest::testSetUri10()
{
  UriStruct us;
  bool v = parse(us, "");

  REQUIRE(!v);
}

void UriTest::testSetUri11()
{
  UriStruct us;
  bool v = parse(us, "http://host?query/");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ(std::string("host"), us.host);
  REQUIRE_EQ(std::string("/"), us.dir);
  REQUIRE_EQ(std::string(""), us.file);
  REQUIRE_EQ(std::string("?query/"), us.query);
}

void UriTest::testSetUri12()
{
  UriStruct us;
  bool v = parse(us, "http://host?query");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ(std::string("host"), us.host);
  REQUIRE_EQ(std::string("/"), us.dir);
  REQUIRE_EQ(std::string(""), us.file);
  REQUIRE_EQ(std::string("?query"), us.query);
}

void UriTest::testSetUri13()
{
  UriStruct us;
  bool v = parse(us, "http://host/?query");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ(std::string("host"), us.host);
  REQUIRE_EQ(std::string("/"), us.dir);
  REQUIRE_EQ(std::string(""), us.file);
  REQUIRE_EQ(std::string("?query"), us.query);
}

void UriTest::testSetUri14()
{
  UriStruct us;
  bool v = parse(us, "http://host:8080/abc?query");

  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ(std::string("host"), us.host);
  REQUIRE_EQ((uint16_t)8080, us.port);
  REQUIRE_EQ(std::string("/"), us.dir);
  REQUIRE_EQ(std::string("abc"), us.file);
  REQUIRE_EQ(std::string("?query"), us.query);
}

void UriTest::testSetUri15()
{
  UriStruct us;
  // 2 slashes after host name and dir
  bool v = parse(us, "http://host//dir1/dir2//file");
  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ(std::string("host"), us.host);
  REQUIRE_EQ(std::string("//dir1/dir2//"), us.dir);
  REQUIRE_EQ(std::string("file"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri16()
{
  UriStruct us;
  // 2 slashes before file
  bool v = parse(us, "http://host//file");
  REQUIRE(v);
  REQUIRE_EQ(std::string("http"), us.protocol);
  REQUIRE_EQ(std::string("host"), us.host);
  REQUIRE_EQ(std::string("//"), us.dir);
  REQUIRE_EQ(std::string("file"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testSetUri18()
{
  UriStruct us;
  bool v = parse(us, "http://1/");

  REQUIRE(v);
}

void UriTest::testSetUri19()
{
  UriStruct us;
  // No host
  bool v = parse(us, "http://user@");

  REQUIRE(!v);
}

void UriTest::testSetUri20()
{
  UriStruct us;
  bool v;
  // Invalid port
  v = parse(us, "http://localhost:65536");
  REQUIRE(!v);
  v = parse(us, "http://localhost:65535");
  REQUIRE(v);
  v = parse(us, "http://localhost:-80");
  REQUIRE(!v);
}

void UriTest::testSetUri_zeroUsername()
{
  UriStruct us;
  REQUIRE(!parse(us, "ftp://@localhost/download/aria2-1.0.0.tar.bz2"));

  REQUIRE(!parse(us, "ftp://:@localhost/download/aria2-1.0.0.tar.bz2"));

  REQUIRE(
      !parse(us, "ftp://:pass@localhost/download/aria2-1.0.0.tar.bz2"));
}

void UriTest::testSetUri_username()
{
  UriStruct us;
  REQUIRE(
      parse(us, "ftp://aria2@user@localhost/download/aria2-1.0.0.tar.bz2"));
  REQUIRE_EQ(std::string("ftp"), us.protocol);
  REQUIRE_EQ((uint16_t)21, us.port);
  REQUIRE_EQ(std::string("localhost"), us.host);
  REQUIRE_EQ(std::string("/download/"), us.dir);
  REQUIRE_EQ(std::string("aria2-1.0.0.tar.bz2"), us.file);
  REQUIRE_EQ(std::string("aria2@user"), us.username);
  REQUIRE_EQ(std::string(""), us.password);
}

void UriTest::testSetUri_usernamePassword()
{
  UriStruct us;
  REQUIRE(parse(us,
                       "ftp://aria2@user%40:aria2@pass%40@localhost/download/"
                       "aria2-1.0.0.tar.bz2"));
  REQUIRE_EQ(std::string("ftp"), us.protocol);
  REQUIRE_EQ((uint16_t)21, us.port);
  REQUIRE_EQ(std::string("pass%40@localhost"), us.host);
  REQUIRE_EQ(std::string("/download/"), us.dir);
  REQUIRE_EQ(std::string("aria2-1.0.0.tar.bz2"), us.file);
  REQUIRE_EQ(std::string("aria2@user@"), us.username);
  REQUIRE_EQ(std::string("aria2"), us.password);

  // make sure that after new uri is set, username and password are updated.
  REQUIRE(parse(us, "ftp://localhost/download/aria2-1.0.0.tar.bz2"));
  REQUIRE_EQ(std::string(""), us.username);
  REQUIRE_EQ(std::string(""), us.password);
}

void UriTest::testSetUri_ipv6()
{
  UriStruct us;
  REQUIRE(!parse(us, "http://[::1"));
  REQUIRE(parse(us, "http://[::1]"));
  REQUIRE_EQ(std::string("::1"), us.host);

  REQUIRE(parse(us, "http://[::1]:8000/dir/file"));
  REQUIRE_EQ(std::string("::1"), us.host);
  REQUIRE_EQ((uint16_t)8000, us.port);
  REQUIRE_EQ(std::string("/dir/"), us.dir);
  REQUIRE_EQ(std::string("file"), us.file);
  REQUIRE(us.ipv6LiteralAddress);
}

void UriTest::testInnerLink()
{
  UriStruct us;
  bool v = parse(us, "http://aria.rednoah.com/index.html#download");
  REQUIRE(v);
  REQUIRE_EQ(std::string("index.html"), us.file);
  REQUIRE_EQ(std::string(""), us.query);
}

void UriTest::testConstruct()
{
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host/dir/file?q=abc#foo"));
    REQUIRE_EQ(std::string("http://host/dir/file?q=abc"),
                         construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host/dir/file"));
    REQUIRE_EQ(std::string("http://host/dir/file"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host/dir/"));
    REQUIRE_EQ(std::string("http://host/dir/"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host/dir"));
    REQUIRE_EQ(std::string("http://host/dir"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host/"));
    REQUIRE_EQ(std::string("http://host/"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host"));
    REQUIRE_EQ(std::string("http://host/"), construct(us));
  }
  {
    UriStruct us;
    us.protocol = "http";
    us.host = "host";
    us.file = "foo.xml";
    REQUIRE_EQ(std::string("http://host/foo.xml"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host:80"));
    REQUIRE_EQ(std::string("http://host/"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://host:8080"));
    REQUIRE_EQ(std::string("http://host:8080/"), construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "http://[::1]:8000/dir/file"));
    REQUIRE_EQ(std::string("http://[::1]:8000/dir/file"),
                         construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "ftp://user%40@host/dir/file"));
    REQUIRE_EQ(std::string("ftp://user%40@host/dir/file"),
                         construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "ftp://user:@host/dir/file"));
    REQUIRE_EQ(std::string("ftp://user:@host/dir/file"),
                         construct(us));
  }
  {
    UriStruct us;
    REQUIRE(parse(us, "ftp://user:passwd%40@host/dir/file"));
    REQUIRE_EQ(std::string("ftp://user:passwd%40@host/dir/file"),
                         construct(us));
  }
}

void UriTest::testSwap()
{
  UriStruct us1;
  REQUIRE(parse(us1, "http://u1:p1@[::1]/dir1/file1?k1=v1"));
  UriStruct us2;
  REQUIRE(parse(us2, "ftp://host2/dir2/file2?k2=v2"));
  us1.swap(us2);
  REQUIRE_EQ(std::string("ftp://host2/dir2/file2?k2=v2"),
                       construct(us1));
  REQUIRE_EQ(std::string("http://u1:p1@[::1]/dir1/file1?k1=v1"),
                       construct(us2));
}

void UriTest::testJoinUri()
{
  REQUIRE_EQ(std::string("http://host/dir/file"),
                       joinUri("http://base/d/f", "http://host/dir/file"));

  REQUIRE_EQ(std::string("http://base/dir/file"),
                       joinUri("http://base/d/f", "/dir/file"));

  REQUIRE_EQ(std::string("http://base/d/dir/file"),
                       joinUri("http://base/d/f", "dir/file"));

  REQUIRE_EQ(std::string("http://base/d/"),
                       joinUri("http://base/d/f", ""));

  REQUIRE_EQ(std::string("http://base/d/dir/file?q=k"),
                       joinUri("http://base/d/f", "dir/file?q=k"));

  REQUIRE_EQ(std::string("dir/file"), joinUri("baduri", "dir/file"));

  REQUIRE_EQ(std::string("http://base/a/b/d/file"),
                       joinUri("http://base/a/b/c/x", "../d/file"));

  REQUIRE_EQ(std::string("http://base/a/b/file"),
                       joinUri("http://base/c/x", "../../a/b/file"));

  REQUIRE_EQ(std::string("http://base/"),
                       joinUri("http://base/c/x", "../.."));

  REQUIRE_EQ(std::string("http://base/"),
                       joinUri("http://base/c/x", ".."));

  REQUIRE_EQ(std::string("http://base/a/file"),
                       joinUri("http://base/b/c/x", "/a/x/../file"));

  REQUIRE_EQ(std::string("http://base/file"),
                       joinUri("http://base/f/?q=k", "/file"));

  REQUIRE_EQ(std::string("http://base/file?q=/"),
                       joinUri("http://base/", "/file?q=/"));

  REQUIRE_EQ(std::string("http://base/file?q=v"),
                       joinUri("http://base/", "/file?q=v#a?q=x"));

  REQUIRE_EQ(std::string("http://base/file"),
                       joinUri("http://base/", "/file#a?q=x"));
}

void UriTest::testJoinPath()
{
  REQUIRE_EQ(std::string("/b"), joinPath("/a", "/b"));
  REQUIRE_EQ(std::string("/alpha/bravo"),
                       joinPath("/alpha", "bravo"));
  REQUIRE_EQ(std::string("/bravo"),
                       joinPath("/a", "/alpha/../bravo"));
  REQUIRE_EQ(std::string("/alpha/charlie/"),
                       joinPath("/a", "/alpha/bravo/../charlie/"));
  REQUIRE_EQ(std::string("/alpha/bravo/"),
                       joinPath("/a", "/alpha////bravo//"));
  REQUIRE_EQ(std::string("/alpha/bravo/"),
                       joinPath("/a", "/alpha/././bravo/"));
  REQUIRE_EQ(std::string("/alpha/bravo/"),
                       joinPath("/a", "/alpha/bravo/./"));
  REQUIRE_EQ(std::string("/alpha/bravo/"),
                       joinPath("/a", "/alpha/bravo/."));
  REQUIRE_EQ(std::string("/alpha/"),
                       joinPath("/a", "/alpha/bravo/.."));
  REQUIRE_EQ(std::string("/alpha/"), joinPath("/", "../alpha/"));
  REQUIRE_EQ(std::string("/bravo/"), joinPath("/alpha", "../bravo/"));
  REQUIRE_EQ(std::string("/bravo/"),
                       joinPath("/alpha", "../../bravo/"));
  // If neither paths do not start with '/', the resulting path also
  // does not start with '/'.
  REQUIRE_EQ(std::string("alpha/bravo"), joinPath("alpha", "bravo"));
  REQUIRE_EQ(std::string("bravo/"),
                       joinPath("alpha", "../../bravo/"));
}

} // namespace uri

} // namespace aria2
