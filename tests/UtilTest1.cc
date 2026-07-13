#include "util.h"

#include <cmath>
#include <cstring>
#include <string>
#include <cassert>
#include <iostream>

#include "a2doctest.h"

#include "FixedNumberRandomizer.h"
#include "DlAbortEx.h"
#include "BitfieldMan.h"
#include "ByteArrayDiskWriter.h"
#include "FileEntry.h"
#include "File.h"
#include "array_fun.h"
#include "BufferedFile.h"
#include "TestUtil.h"
#include "SocketCore.h"

namespace aria2 {

class UtilTest1 {


private:
public:
  void setUp() {}

  void testStrip();
  void testStripIter();
  void testLstripIter();
  void testLstripIter_char();
  void testDivide();
  void testSplit();
  void testSplitIter();
  void testSplitIterM();
  void testStreq();
  void testStrieq();
  void testStrifind();
  void testEndsWith();
  void testIendsWith();
  void testReplace();
  void testStartsWith();
  void testIstartsWith();
  // may be moved to other helper class in the future.
  void testGetContentDispositionFilename();
  void testParseContentDisposition1();
  void testParseContentDisposition2();
};

A2_TEST(UtilTest1, testStrip)
A2_TEST(UtilTest1, testStripIter)
A2_TEST(UtilTest1, testLstripIter)
A2_TEST(UtilTest1, testLstripIter_char)
A2_TEST(UtilTest1, testDivide)
A2_TEST(UtilTest1, testSplit)
A2_TEST(UtilTest1, testSplitIter)
A2_TEST(UtilTest1, testSplitIterM)
A2_TEST(UtilTest1, testStreq)
A2_TEST(UtilTest1, testStrieq)
A2_TEST(UtilTest1, testStrifind)
A2_TEST(UtilTest1, testEndsWith)
A2_TEST(UtilTest1, testIendsWith)
A2_TEST(UtilTest1, testReplace)
A2_TEST(UtilTest1, testStartsWith)
A2_TEST(UtilTest1, testIstartsWith)
A2_TEST(UtilTest1, testGetContentDispositionFilename)
A2_TEST(UtilTest1, testParseContentDisposition1)
A2_TEST(UtilTest1, testParseContentDisposition2)

void UtilTest1::testStrip()
{
  std::string str1 = "aria2";
  REQUIRE_EQ(str1, util::strip("aria2"));
  REQUIRE_EQ(str1, util::strip(" aria2"));
  REQUIRE_EQ(str1, util::strip("aria2 "));
  REQUIRE_EQ(str1, util::strip(" aria2 "));
  REQUIRE_EQ(str1, util::strip("  aria2  "));
  std::string str2 = "aria2 debut";
  REQUIRE_EQ(str2, util::strip("aria2 debut"));
  REQUIRE_EQ(str2, util::strip(" aria2 debut "));
  std::string str3 = "";
  REQUIRE_EQ(str3, util::strip(""));
  REQUIRE_EQ(str3, util::strip(" "));
  REQUIRE_EQ(str3, util::strip("  "));
  std::string str4 = "A";
  REQUIRE_EQ(str4, util::strip("A"));
  REQUIRE_EQ(str4, util::strip(" A "));
  REQUIRE_EQ(str4, util::strip("  A  "));
}

void UtilTest1::testStripIter()
{
  Scip p;
  std::string str1 = "aria2";
  std::string s = "aria2";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str1, std::string(p.first, p.second));
  s = " aria2";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str1, std::string(p.first, p.second));
  s = "aria2 ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str1, std::string(p.first, p.second));
  s = " aria2 ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str1, std::string(p.first, p.second));
  s = "  aria2  ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str1, std::string(p.first, p.second));
  std::string str2 = "aria2 debut";
  s = "aria2 debut";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str2, std::string(p.first, p.second));
  s = " aria2 debut ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str2, std::string(p.first, p.second));
  std::string str3 = "";
  s = "";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str3, std::string(p.first, p.second));
  s = " ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str3, std::string(p.first, p.second));
  s = "  ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str3, std::string(p.first, p.second));
  std::string str4 = "A";
  s = "A";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str4, std::string(p.first, p.second));
  s = " A ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str4, std::string(p.first, p.second));
  s = "  A  ";
  p = util::stripIter(s.begin(), s.end());
  REQUIRE_EQ(str4, std::string(p.first, p.second));
}

void UtilTest1::testLstripIter()
{
  std::string::iterator r;
  std::string s = "foo";
  r = util::lstripIter(s.begin(), s.end());
  REQUIRE_EQ(std::string("foo"), std::string(r, s.end()));

  s = "  foo bar  ";
  r = util::lstripIter(s.begin(), s.end());
  REQUIRE_EQ(std::string("foo bar  "), std::string(r, s.end()));

  s = "f";
  r = util::lstripIter(s.begin(), s.end());
  REQUIRE_EQ(std::string("f"), std::string(r, s.end()));

  s = "foo  ";
  r = util::lstripIter(s.begin(), s.end());
  REQUIRE_EQ(std::string("foo  "), std::string(r, s.end()));
}

void UtilTest1::testLstripIter_char()
{
  std::string::iterator r;
  std::string s = "foo";
  r = util::lstripIter(s.begin(), s.end(), '$');
  REQUIRE_EQ(std::string("foo"), std::string(r, s.end()));

  s = "$$foo$bar$$";
  r = util::lstripIter(s.begin(), s.end(), '$');
  REQUIRE_EQ(std::string("foo$bar$$"), std::string(r, s.end()));

  s = "f";
  r = util::lstripIter(s.begin(), s.end(), '$');
  REQUIRE_EQ(std::string("f"), std::string(r, s.end()));

  s = "foo$$";
  r = util::lstripIter(s.begin(), s.end(), '$');
  REQUIRE_EQ(std::string("foo$$"), std::string(r, s.end()));
}

void UtilTest1::testDivide()
{
  std::string s = "name=value";
  auto p1 = util::divide(std::begin(s), std::end(s), '=');
  REQUIRE_EQ(std::string("name"),
                       std::string(p1.first.first, p1.first.second));
  REQUIRE_EQ(std::string("value"),
                       std::string(p1.second.first, p1.second.second));
  s = " name = value ";
  p1 = util::divide(std::begin(s), std::end(s), '=');
  REQUIRE_EQ(std::string("name"),
                       std::string(p1.first.first, p1.first.second));
  REQUIRE_EQ(std::string("value"),
                       std::string(p1.second.first, p1.second.second));
  s = "=value";
  p1 = util::divide(std::begin(s), std::end(s), '=');
  REQUIRE_EQ(std::string(""),
                       std::string(p1.first.first, p1.first.second));
  REQUIRE_EQ(std::string("value"),
                       std::string(p1.second.first, p1.second.second));
  s = "name=";
  p1 = util::divide(std::begin(s), std::end(s), '=');
  REQUIRE_EQ(std::string("name"),
                       std::string(p1.first.first, p1.first.second));
  REQUIRE_EQ(std::string(""),
                       std::string(p1.second.first, p1.second.second));
  s = "name";
  p1 = util::divide(std::begin(s), std::end(s), '=');
  REQUIRE_EQ(std::string("name"),
                       std::string(p1.first.first, p1.first.second));
  REQUIRE_EQ(std::string(""),
                       std::string(p1.second.first, p1.second.second));
}

void UtilTest1::testSplit()
{
  std::vector<std::string> v;
  std::string s = "k1; k2;; k3";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', true);
  REQUIRE_EQ((size_t)3, v.size());
  std::vector<std::string>::iterator itr = v.begin();
  REQUIRE_EQ(std::string("k1"), *itr++);
  REQUIRE_EQ(std::string("k2"), *itr++);
  REQUIRE_EQ(std::string("k3"), *itr++);

  v.clear();

  s = "k1; k2; k3";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)3, v.size());
  itr = v.begin();
  REQUIRE_EQ(std::string("k1"), *itr++);
  REQUIRE_EQ(std::string(" k2"), *itr++);
  REQUIRE_EQ(std::string(" k3"), *itr++);

  v.clear();

  s = "k=v";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)1, v.size());
  itr = v.begin();
  REQUIRE_EQ(std::string("k=v"), *itr++);

  v.clear();

  s = ";;k1;;k2;";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)6, v.size());
  itr = v.begin();
  REQUIRE_EQ(std::string(""), *itr++);
  REQUIRE_EQ(std::string(""), *itr++);
  REQUIRE_EQ(std::string("k1"), *itr++);
  REQUIRE_EQ(std::string(""), *itr++);
  REQUIRE_EQ(std::string("k2"), *itr++);
  REQUIRE_EQ(std::string(""), *itr++);

  v.clear();

  s = ";;k1;;k2;";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)2, v.size());
  itr = v.begin();
  REQUIRE_EQ(std::string("k1"), *itr++);
  REQUIRE_EQ(std::string("k2"), *itr++);

  v.clear();

  s = "k; ";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)2, v.size());
  itr = v.begin();
  REQUIRE_EQ(std::string("k"), *itr++);
  REQUIRE_EQ(std::string(" "), *itr++);

  v.clear();

  s = " ";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', true, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(""), v[0]);

  v.clear();

  s = " ";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', true);
  REQUIRE_EQ((size_t)0, v.size());

  v.clear();

  s = " ";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(" "), v[0]);

  v.clear();

  s = ";";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)0, v.size());

  v.clear();

  s = ";";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)2, v.size());
  itr = v.begin();
  REQUIRE_EQ(std::string(""), *itr++);
  REQUIRE_EQ(std::string(""), *itr++);

  v.clear();

  s = "";
  util::split(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(""), v[0]);
}

void UtilTest1::testSplitIter()
{
  std::vector<Scip> v;
  std::string s = "k1; k2;; k3";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', true);
  REQUIRE_EQ((size_t)3, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string("k3"), std::string(v[2].first, v[2].second));

  v.clear();

  s = "k1; k2; k3";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)3, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(" k2"),
                       std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string(" k3"),
                       std::string(v[2].first, v[2].second));

  v.clear();

  s = "k=v";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string("k=v"),
                       std::string(v[0].first, v[0].second));

  v.clear();

  s = ";;k1;;k2;";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)6, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(""), std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string("k1"), std::string(v[2].first, v[2].second));
  REQUIRE_EQ(std::string(""), std::string(v[3].first, v[3].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[4].first, v[4].second));
  REQUIRE_EQ(std::string(""), std::string(v[5].first, v[5].second));

  v.clear();

  s = ";;k1;;k2;";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)2, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[1].first, v[1].second));

  v.clear();

  s = "k; ";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)2, v.size());
  REQUIRE_EQ(std::string("k"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(" "), std::string(v[1].first, v[1].second));

  v.clear();

  s = " ";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', true, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));

  v.clear();

  s = " ";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', true);
  REQUIRE_EQ((size_t)0, v.size());

  v.clear();

  s = " ";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(" "), std::string(v[0].first, v[0].second));

  v.clear();

  s = ";";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';');
  REQUIRE_EQ((size_t)0, v.size());

  v.clear();

  s = ";";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)2, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(""), std::string(v[1].first, v[1].second));

  v.clear();

  s = "";
  util::splitIter(s.begin(), s.end(), std::back_inserter(v), ';', false, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));
}

void UtilTest1::testSplitIterM()
{
  const char d[] = ";";
  const char md[] = "; ";
  std::vector<Scip> v;
  std::string s = "k1; k2;; k3";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, true);
  REQUIRE_EQ((size_t)3, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string("k3"), std::string(v[2].first, v[2].second));

  v.clear();

  s = "k1; k2; k3";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d);
  REQUIRE_EQ((size_t)3, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(" k2"),
                       std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string(" k3"),
                       std::string(v[2].first, v[2].second));

  v.clear();

  s = "k1; k2; k3";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), md);
  REQUIRE_EQ((size_t)3, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string("k3"), std::string(v[2].first, v[2].second));

  v.clear();

  s = "k1; k2; k3;";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), md, false, true);
  REQUIRE_EQ((size_t)6, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(""), std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[2].first, v[2].second));
  REQUIRE_EQ(std::string(""), std::string(v[3].first, v[3].second));
  REQUIRE_EQ(std::string("k3"), std::string(v[4].first, v[4].second));
  REQUIRE_EQ(std::string(""), std::string(v[5].first, v[5].second));

  v.clear();

  s = "k=v";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, false, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string("k=v"),
                       std::string(v[0].first, v[0].second));

  v.clear();

  s = ";;k1;;k2;";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, false, true);
  REQUIRE_EQ((size_t)6, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(""), std::string(v[1].first, v[1].second));
  REQUIRE_EQ(std::string("k1"), std::string(v[2].first, v[2].second));
  REQUIRE_EQ(std::string(""), std::string(v[3].first, v[3].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[4].first, v[4].second));
  REQUIRE_EQ(std::string(""), std::string(v[5].first, v[5].second));

  v.clear();

  s = ";;k1;;k2;";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d);
  REQUIRE_EQ((size_t)2, v.size());
  REQUIRE_EQ(std::string("k1"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string("k2"), std::string(v[1].first, v[1].second));

  v.clear();

  s = "k; ";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d);
  REQUIRE_EQ((size_t)2, v.size());
  REQUIRE_EQ(std::string("k"), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(" "), std::string(v[1].first, v[1].second));

  v.clear();

  s = " ";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, true, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));

  v.clear();

  s = " ";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, true);
  REQUIRE_EQ((size_t)0, v.size());

  v.clear();

  s = " ";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(" "), std::string(v[0].first, v[0].second));

  v.clear();

  s = ";";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d);
  REQUIRE_EQ((size_t)0, v.size());

  v.clear();

  s = ";";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, false, true);
  REQUIRE_EQ((size_t)2, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));
  REQUIRE_EQ(std::string(""), std::string(v[1].first, v[1].second));

  v.clear();

  s = "";
  util::splitIterM(s.begin(), s.end(), std::back_inserter(v), d, false, true);
  REQUIRE_EQ((size_t)1, v.size());
  REQUIRE_EQ(std::string(""), std::string(v[0].first, v[0].second));
}

void UtilTest1::testEndsWith()
{
  std::string target = "abcdefg";
  std::string part = "fg";
  REQUIRE(
      util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "abdefg";
  part = "g";
  REQUIRE(
      util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "abdefg";
  part = "eg";
  REQUIRE(
      !util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "g";
  part = "eg";
  REQUIRE(
      !util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "g";
  part = "g";
  REQUIRE(
      util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "g";
  part = "";
  REQUIRE(
      util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "";
  part = "";
  REQUIRE(
      util::endsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "";
  part = "g";
  REQUIRE(
      !util::endsWith(target.begin(), target.end(), part.begin(), part.end()));
}

void UtilTest1::testIendsWith()
{
  std::string target = "abcdefg";
  std::string part = "Fg";
  REQUIRE(
      util::iendsWith(target.begin(), target.end(), part.begin(), part.end()));

  target = "abdefg";
  part = "ef";
  REQUIRE(
      !util::iendsWith(target.begin(), target.end(), part.begin(), part.end()));
}

void UtilTest1::testStreq()
{
  std::string s1, s2;
  s1 = "foo";
  s2 = "foo";
  REQUIRE(util::streq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(util::streq(s1.begin(), s1.end(), s2.c_str()));

  s2 = "fooo";
  REQUIRE(!util::streq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(!util::streq(s1.begin(), s1.end(), s2.c_str()));

  s2 = "fo";
  REQUIRE(!util::streq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(!util::streq(s1.begin(), s1.end(), s2.c_str()));

  s2 = "";
  REQUIRE(!util::streq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(!util::streq(s1.begin(), s1.end(), s2.c_str()));

  s1 = "";
  REQUIRE(util::streq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(util::streq(s1.begin(), s1.end(), s2.c_str()));
}

void UtilTest1::testStrieq()
{
  std::string s1, s2;
  s1 = "foo";
  s2 = "foo";
  REQUIRE(util::strieq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(util::strieq(s1.begin(), s1.end(), s2.c_str()));

  s1 = "FoO";
  s2 = "fOo";
  REQUIRE(util::strieq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(util::strieq(s1.begin(), s1.end(), s2.c_str()));

  s2 = "fooo";
  REQUIRE(!util::strieq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(!util::strieq(s1.begin(), s1.end(), s2.c_str()));

  s2 = "fo";
  REQUIRE(!util::strieq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(!util::strieq(s1.begin(), s1.end(), s2.c_str()));

  s2 = "";
  REQUIRE(!util::strieq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(!util::strieq(s1.begin(), s1.end(), s2.c_str()));

  s1 = "";
  REQUIRE(util::strieq(s1.begin(), s1.end(), s2.begin(), s2.end()));
  REQUIRE(util::strieq(s1.begin(), s1.end(), s2.c_str()));
}

void UtilTest1::testStrifind()
{
  std::string s1, s2;
  s1 = "yamagakani mukashi wo toheba hARU no tuki";
  s2 = "HaRu";
  REQUIRE(util::strifind(s1.begin(), s1.end(), s2.begin(), s2.end()) !=
                 s1.end());
  s2 = "aki";
  REQUIRE(util::strifind(s1.begin(), s1.end(), s2.begin(), s2.end()) ==
                 s1.end());
  s1 = "h";
  s2 = "HH";
  REQUIRE(util::strifind(s1.begin(), s1.end(), s2.begin(), s2.end()) ==
                 s1.end());
}

void UtilTest1::testReplace()
{
  REQUIRE_EQ(std::string("abc\n"),
                       util::replace("abc\r\n", "\r", ""));
  REQUIRE_EQ(std::string("abc"),
                       util::replace("abc\r\n", "\r\n", ""));
  REQUIRE_EQ(std::string(""), util::replace("", "\r\n", ""));
  REQUIRE_EQ(std::string("abc"), util::replace("abc", "", "a"));
  REQUIRE_EQ(std::string("xbc"), util::replace("abc", "a", "x"));
}

void UtilTest1::testStartsWith()
{
  std::string target;
  std::string part;

  target = "abcdefg";
  part = "abc";
  REQUIRE(
      util::startsWith(target.begin(), target.end(), part.begin(), part.end()));
  REQUIRE(util::startsWith(target.begin(), target.end(), part.c_str()));

  target = "abcdefg";
  part = "abx";
  REQUIRE(!util::startsWith(target.begin(), target.end(), part.begin(),
                                   part.end()));
  REQUIRE(!util::startsWith(target.begin(), target.end(), part.c_str()));

  target = "abcdefg";
  part = "bcd";
  REQUIRE(!util::startsWith(target.begin(), target.end(), part.begin(),
                                   part.end()));
  REQUIRE(!util::startsWith(target.begin(), target.end(), part.c_str()));

  target = "";
  part = "a";
  REQUIRE(!util::startsWith(target.begin(), target.end(), part.begin(),
                                   part.end()));
  REQUIRE(!util::startsWith(target.begin(), target.end(), part.c_str()));

  target = "";
  part = "";
  REQUIRE(
      util::startsWith(target.begin(), target.end(), part.begin(), part.end()));
  REQUIRE(util::startsWith(target.begin(), target.end(), part.c_str()));

  target = "a";
  part = "";
  REQUIRE(
      util::startsWith(target.begin(), target.end(), part.begin(), part.end()));
  REQUIRE(util::startsWith(target.begin(), target.end(), part.c_str()));

  target = "a";
  part = "a";
  REQUIRE(
      util::startsWith(target.begin(), target.end(), part.begin(), part.end()));
  REQUIRE(util::startsWith(target.begin(), target.end(), part.c_str()));
}

void UtilTest1::testIstartsWith()
{
  std::string target;
  std::string part;

  target = "abcdefg";
  part = "aBc";
  REQUIRE(util::istartsWith(target.begin(), target.end(), part.begin(),
                                   part.end()));
  REQUIRE(util::istartsWith(target.begin(), target.end(), part.c_str()));

  target = "abcdefg";
  part = "abx";
  REQUIRE(!util::istartsWith(target.begin(), target.end(), part.begin(),
                                    part.end()));
  REQUIRE(
      !util::istartsWith(target.begin(), target.end(), part.c_str()));
}

void UtilTest1::testGetContentDispositionFilename()
{
  std::string val;

  val = "attachment; filename=\"aria2.tar.bz2\"";
  REQUIRE_EQ(std::string("aria2.tar.bz2"),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"aria2.tar.bz2\";";
  REQUIRE_EQ(std::string("aria2.tar.bz2"),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=aria2.tar.bz2;";
  REQUIRE_EQ(std::string("aria2.tar.bz2"),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename= \" aria2.tar.bz2 \"";
  REQUIRE_EQ(std::string(" aria2.tar.bz2 "),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=dir/file";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=dir\\file";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"dir/file\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"dir\\\\file\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"/etc/passwd\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=\"..\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename=..";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, false));

  // Unescaping %2E%2E%2F produces "../". But since we won't unescape,
  // we just accept it as is.
  val = "attachment; filename=\"%2E%2E%2Ffoo.html\"";
  REQUIRE_EQ(std::string("%2E%2E%2Ffoo.html"),
                       util::getContentDispositionFilename(val, false));

  // iso-8859-1 string will be converted to utf-8.
  val = "attachment; filename*=iso-8859-1''foo-%E4.html";
  REQUIRE_EQ(std::string("foo-ä.html"),
                       util::getContentDispositionFilename(val, false));

  val = "attachment; filename*= UTF-8''foo-%c3%a4.html";
  REQUIRE_EQ(std::string("foo-ä.html"),
                       util::getContentDispositionFilename(val, false));

  // iso-8859-1 string will be converted to utf-8.
  val = "attachment; filename=\"foo-%E4.html\"";
  val = util::percentDecode(val.begin(), val.end());
  REQUIRE_EQ(std::string("foo-ä.html"),
                       util::getContentDispositionFilename(val, false));

  // allow utf-8 in filename if default_utf8 is set.
  val = "attachment; filename=\"foo-ä.html\"";
  REQUIRE_EQ(std::string("foo-ä.html"),
                       util::getContentDispositionFilename(val, true));

  // return empty if default_utf8 is set but invalid utf8.
  val = "attachment; filename=\"foo-\xc2\x02.html\"";
  REQUIRE_EQ(std::string(""),
                       util::getContentDispositionFilename(val, true));
}

void UtilTest1::testParseContentDisposition1()
{
  char dest[1_k];
  size_t destlen = sizeof(dest);
  const char* cs;
  size_t cslen;
  std::string val;

  // test cases from http://greenbytes.de/tech/tc2231/
  // inlonly
  val = "inline";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // inlonlyquoted
  val = "\"inline\"";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // inlwithasciifilename
  val = "inline; filename=\"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // inlwithfnattach
  val = "inline; filename=\"Not an attachment!\"";
  REQUIRE_EQ((ssize_t)18, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("Not an attachment!"),
                       std::string(&dest[0], &dest[18]));

  // inlwithasciifilenamepdf
  val = "inline; filename=\"foo.pdf\"";
  REQUIRE_EQ((ssize_t)7, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.pdf"), std::string(&dest[0], &dest[7]));

  // attwithasciifilename25
  val = "attachment; filename=\"0000000000111111111122222\"";
  REQUIRE_EQ((ssize_t)25, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("0000000000111111111122222"),
                       std::string(&dest[0], &dest[25]));

  // attwithasciifilename35
  val = "attachment; filename=\"00000000001111111111222222222233333\"";
  REQUIRE_EQ((ssize_t)35, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("00000000001111111111222222222233333"),
                       std::string(&dest[0], &dest[35]));

  // attwithasciifnescapedchar
  val = "attachment; filename=\"f\\oo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attwithasciifnescapedquote
  val = "attachment; filename=\"\\\"quoting\\\" tested.html\"";
  REQUIRE_EQ((ssize_t)21, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("\"quoting\" tested.html"),
                       std::string(&dest[0], &dest[21]));

  // attwithquotedsemicolon
  val = "attachment; filename=\"Here's a semicolon;.html\"";
  REQUIRE_EQ((ssize_t)24, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("Here's a semicolon;.html"),
                       std::string(&dest[0], &dest[24]));

  // attwithfilenameandextparam
  val = "attachment; foo=\"bar\"; filename=\"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attwithfilenameandextparamescaped
  val = "attachment; foo=\"\\\"\\\\\";filename=\"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attwithasciifilenameucase
  val = "attachment; FILENAME=\"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attwithasciifilenamenq
  val = "attachment; filename=foo.html";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attwithtokfncommanq
  val = "attachment; filename=foo,bar.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithasciifilenamenqs
  val = "attachment; filename=foo.html ;";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attemptyparam
  val = "attachment; ;filename=foo";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithasciifilenamenqws
  val = "attachment; filename=foo bar.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfntokensq
  val = "attachment; filename='foo.bar'";
  REQUIRE_EQ((ssize_t)9, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("'foo.bar'"),
                       std::string(&dest[0], &dest[9]));

  // attwithisofnplain
  // attachment; filename="foo-ä.html"
  val = "attachment; filename=\"foo-%E4.html\"";
  val = util::percentDecode(val.begin(), val.end());
  REQUIRE_EQ((ssize_t)10, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       util::iso8859p1ToUtf8(std::string(&dest[0], &dest[10])));

  // attwithutf8fnplain
  // attachment; filename="foo-Ã¤.html"
  val = "attachment; filename=\"foo-%C3%A4.html\"";
  val = util::percentDecode(val.begin(), val.end());
  REQUIRE_EQ((ssize_t)11, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-Ã¤.html"),
                       util::iso8859p1ToUtf8(std::string(&dest[0], &dest[11])));

  // attwithfnrawpctenca
  val = "attachment; filename=\"foo-%41.html\"";
  REQUIRE_EQ((ssize_t)12, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-%41.html"),
                       std::string(&dest[0], &dest[12]));

  // attwithfnusingpct
  val = "attachment; filename=\"50%.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("50%.html"),
                       std::string(&dest[0], &dest[8]));

  // attwithfnrawpctencaq
  val = "attachment; filename=\"foo-%\\41.html\"";
  REQUIRE_EQ((ssize_t)12, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-%41.html"),
                       std::string(&dest[0], &dest[12]));

  // attwithnamepct
  val = "attachment; name=\"foo-%41.html\"";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // attwithfilenamepctandiso
  // attachment; filename="ä-%41.html"
  val = "attachment; filename=\"%E4-%2541.html\"";
  val = util::percentDecode(val.begin(), val.end());
  REQUIRE_EQ((ssize_t)10, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("ä-%41.html"),
                       util::iso8859p1ToUtf8(std::string(&dest[0], &dest[10])));

  // attwithfnrawpctenclong
  val = "attachment; filename=\"foo-%c3%a4-%e2%82%ac.html\"";
  REQUIRE_EQ((ssize_t)25, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-%c3%a4-%e2%82%ac.html"),
                       std::string(&dest[0], &dest[25]));

  // attwithasciifilenamews1
  val = "attachment; filename =\"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attwith2filenames
  val = "attachment; filename=\"foo.html\"; filename=\"bar.html\"";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attfnbrokentoken
  val = "attachment; filename=foo[1](2).html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attfnbrokentokeniso
  val = "attachment; filename=foo-%E4.html";
  val = util::percentDecode(val.begin(), val.end());
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attfnbrokentokenutf
  // attachment; filename=foo-Ã¤.html
  val = "attachment; filename=foo-ä.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmissingdisposition
  val = "filename=foo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmissingdisposition2
  val = "x=y; filename=foo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmissingdisposition3
  val = "\"foo; filename=bar;baz\"; filename=qux";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmissingdisposition4
  val = "filename=foo.html, filename=bar.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // emptydisposition
  val = "; filename=foo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // doublecolon
  val = ": inline; attachment; filename=foo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attandinline
  val = "inline; attachment; filename=foo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attandinline2
  val = "attachment; inline; filename=foo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attbrokenquotedfn
  val = "attachment; filename=\"foo.html\".txt";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attbrokenquotedfn2
  val = "attachment; filename=\"bar";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attbrokenquotedfn3
  val = "attachment; filename=foo\"bar;baz\"qux";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmultinstances
  val = "attachment; filename=foo.html, attachment; filename=bar.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
}

void UtilTest1::testParseContentDisposition2()
{
  char dest[1_k];
  size_t destlen = sizeof(dest);
  const char* cs;
  size_t cslen;
  std::string val;

  // test cases from http://greenbytes.de/tech/tc2231/
  // attmissingdelim
  val = "attachment; foo=foo filename=bar";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmissingdelim2
  val = "attachment; filename=bar foo=foo ";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attmissingdelim3
  val = "attachment filename=bar";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attreversed
  val = "filename=foo.html; attachment";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attconfusedparam
  val = "attachment; xfilename=foo.html";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // attabspath
  val = "attachment; filename=\"/foo.html\"";
  REQUIRE_EQ((ssize_t)9, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("/foo.html"),
                       std::string(&dest[0], &dest[9]));

  // attabspathwin
  val = "attachment; filename=\"\\\\foo.html\"";
  REQUIRE_EQ((ssize_t)9, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("\\foo.html"),
                       std::string(&dest[0], &dest[9]));

  // attcdate
  val = "attachment; creation-date=\"Wed, 12 Feb 1997 16:29:51 -0500\"";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // dispext
  val = "foobar";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // dispextbadfn
  val = "attachment; example=\"filename=example.txt\"";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // attwithisofn2231iso
  val = "attachment; filename*=iso-8859-1''foo-%E4.html";
  REQUIRE_EQ((ssize_t)10, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("iso-8859-1"), std::string(cs, cslen));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       util::iso8859p1ToUtf8(std::string(&dest[0], &dest[10])));

  // attwithfn2231utf8
  val = "attachment; filename*=UTF-8''foo-%c3%a4-%e2%82%ac.html";
  REQUIRE_EQ((ssize_t)15, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("UTF-8"), std::string(cs, cslen));
  REQUIRE_EQ(std::string("foo-ä-€.html"),
                       std::string(&dest[0], &dest[15]));

  // attwithfn2231noc
  val = "attachment; filename*=''foo-%c3%a4-%e2%82%ac.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231utf8comp
  val = "attachment; filename*=UTF-8''foo-a%cc%88.html";
  REQUIRE_EQ((ssize_t)12, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  val = "foo-a%cc%88.html";
  REQUIRE_EQ(std::string(util::percentDecode(val.begin(), val.end())),
                       std::string(&dest[0], &dest[12]));

  // attwithfn2231utf8-bad
  val = "attachment; filename*=iso-8859-1''foo-%c3%a4-%e2%82%ac.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231iso-bad
  val = "attachment; filename*=utf-8''foo-%E4.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231ws1
  val = "attachment; filename *=UTF-8''foo-%c3%a4.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231ws2
  val = "attachment; filename*= UTF-8''foo-%c3%a4.html";
  REQUIRE_EQ((ssize_t)11, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       std::string(&dest[0], &dest[11]));

  // attwithfn2231ws3
  val = "attachment; filename* =UTF-8''foo-%c3%a4.html";
  REQUIRE_EQ((ssize_t)11, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       std::string(&dest[0], &dest[11]));

  // attwithfn2231quot
  val = "attachment; filename*=\"UTF-8''foo-%c3%a4.html\"";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231quot2
  val = "attachment; filename*=\"foo%20bar.html\"";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231singleqmissing
  val = "attachment; filename*=UTF-8'foo-%c3%a4.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231nbadpct1
  val = "attachment; filename*=UTF-8''foo%";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231nbadpct2
  val = "attachment; filename*=UTF-8''f%oo.html";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attwithfn2231dpct
  val = "attachment; filename*=UTF-8''A-%2541.html";
  REQUIRE_EQ((ssize_t)10, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("A-%41.html"),
                       std::string(&dest[0], &dest[10]));

  // attwithfn2231abspathdisguised
  val = "attachment; filename*=UTF-8''%5cfoo.html";
  REQUIRE_EQ((ssize_t)9, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("\\foo.html"),
                       std::string(&dest[0], &dest[9]));

  // attfnboth
  val =
      "attachment; filename=\"foo-ae.html\"; filename*=UTF-8''foo-%c3%a4.html";
  REQUIRE_EQ((ssize_t)11, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       std::string(&dest[0], &dest[11]));

  // attfnboth2
  val =
      "attachment; filename*=UTF-8''foo-%c3%a4.html; filename=\"foo-ae.html\"";
  REQUIRE_EQ((ssize_t)11, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       std::string(&dest[0], &dest[11]));

  // attfnboth3
  val = "attachment; filename*0*=ISO-8859-15''euro-sign%3d%a4; "
        "filename*=ISO-8859-1''currency-sign%3d%a4";
  REQUIRE_EQ((ssize_t)15, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("ISO-8859-1"), std::string(cs, cslen));
  REQUIRE_EQ(std::string("currency-sign=¤"),
                       util::iso8859p1ToUtf8(std::string(&dest[0], &dest[15])));

  // attnewandfn
  val = "attachment; foobar=x; filename=\"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // attrfc2047token
  val = "attachment; filename==?ISO-8859-1?Q?foo-=E4.html?=";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // attrfc2047quoted
  val = "attachment; filename=\"=?ISO-8859-1?Q?foo-=E4.html?=\"";
  REQUIRE_EQ((ssize_t)29, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("=?ISO-8859-1?Q?foo-=E4.html?="),
                       std::string(&dest[0], &dest[29]));

  // aria2 original testcases

  // zero-length filename. token cannot be empty, so this is invalid.
  val = "attachment; filename=";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // zero-length filename. quoted-string can be empty string, so this
  // is ok.
  val = "attachment; filename=\"\"";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));

  // empty value is not allowed
  val = "attachment; filename=;";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // / is not valid char in token.
  val = "attachment; filename=dir/file";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));

  // value-chars is *(pct-encoded / attr-char), so empty string is
  // allowed.
  val = "attachment; filename*=UTF-8''";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("UTF-8"), std::string(cs, cslen));

  val = "attachment; filename*=UTF-8''; filename=foo";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("UTF-8"), std::string(cs, cslen));

  val = "attachment; filename*=UTF-8''  ; filename=foo";
  REQUIRE_EQ((ssize_t)0, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("UTF-8"), std::string(cs, cslen));

  // with language
  val = "attachment; filename*=UTF-8'japanese'konnichiwa";
  REQUIRE_EQ((ssize_t)10, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), false));
  REQUIRE_EQ(std::string("konnichiwa"),
                       std::string(&dest[0], &dest[10]));

  // lws before and after "="
  val = "attachment; filename = foo.html";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // lws before and after "=" with quoted-string
  val = "attachment; filename = \"foo.html\"";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // lws after parm
  val = "attachment; filename=foo.html  ";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  val = "attachment; filename=foo.html ; hello=world";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  val = "attachment; filename=\"foo.html\"  ";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  val = "attachment; filename=\"foo.html\" ; hello=world";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  val = "attachment; filename*=UTF-8''foo.html  ; hello=world";
  REQUIRE_EQ((ssize_t)8, util::parse_content_disposition(
                                       dest, destlen, &cs, &cslen, val.c_str(),
                                       val.size(), false));
  REQUIRE_EQ(std::string("foo.html"),
                       std::string(&dest[0], &dest[8]));

  // allow utf8 if content-disposition-default-utf8 is set
  val = "attachment; filename=\"foo-ä.html\"";
  REQUIRE_EQ((ssize_t)11, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), true));
  REQUIRE_EQ(std::string("foo-ä.html"),
                       std::string(&dest[0], &dest[11]));

  // incomplete utf8 sequence must be rejected
  val = "attachment; filename=\"foo-\xc3.html\"";
  REQUIRE_EQ((ssize_t)-1, util::parse_content_disposition(
                                        dest, destlen, &cs, &cslen, val.c_str(),
                                        val.size(), true));
}

} // namespace aria2
