#include "File.h"
#include "TestUtil.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <fstream>
#include "a2doctest.h"

#include "util.h"

namespace aria2 {

class FileTest {


private:
public:
  void setUp() {}

  void testExists();
  void testIsFile();
  void testIsDir();
  void testRemove();
  void testSize();
  void testMkdir();
  void testGetDirname();
  void testGetBasename();
  void testRenameTo();
  void testUtime();
};

A2_TEST(FileTest, testExists)
A2_TEST(FileTest, testIsFile)
A2_TEST(FileTest, testIsDir)
A2_TEST(FileTest, testRemove)
A2_TEST(FileTest, testSize)
A2_TEST(FileTest, testMkdir)
A2_TEST(FileTest, testGetDirname)
A2_TEST(FileTest, testGetBasename)
A2_TEST(FileTest, testRenameTo)
A2_TEST(FileTest, testUtime)

void FileTest::testExists()
{
  File f(A2_TEST_DIR "/FileTest.cc");
  REQUIRE(f.exists());

  File f2("NonExistentFile");
  REQUIRE(!f2.exists());

  File d1(A2_TEST_DIR);
  REQUIRE(d1.exists());
}

void FileTest::testIsFile()
{
  File f(A2_TEST_DIR "/FileTest.cc");
  REQUIRE(f.isFile());

  File f2("NonExistentFile");
  REQUIRE(!f2.isFile());

  File d1(A2_TEST_DIR);
  REQUIRE(!d1.isFile());
}

void FileTest::testIsDir()
{
  File f(A2_TEST_DIR "/FileTest.cc");
  REQUIRE(!f.isDir());

  File f2("NonExistentFile");
  REQUIRE(!f2.isDir());

  File d1(A2_TEST_DIR);
  REQUIRE(d1.isDir());
}

void FileTest::testRemove()
{
  int fd;
  std::string name = A2_TEST_OUT_DIR "/aria2_FileTest_testRemove_testregfile";
  unlink(name.c_str());
  if ((fd = creat(name.c_str(), S_IRUSR | S_IWUSR)) < 0) {
    FAIL("cannot create test file");
  }
  close(fd);
  File f(name);
  REQUIRE(f.isFile());
  REQUIRE(f.remove());
  REQUIRE(!f.exists());
  // delete the file again
  REQUIRE(!f.remove());

  std::string dir = A2_TEST_OUT_DIR "/aria2_FileTest_testRemove_testdir";
#ifdef __MINGW32__
  mkdir(dir.c_str());
#else
  mkdir(dir.c_str(), 0777);
#endif // __MINGW32__
  File d(dir);
  REQUIRE(d.exists());
  REQUIRE(d.remove());
  REQUIRE(!d.exists());
  // delete the directory again
  REQUIRE(!d.remove());
}

void FileTest::testSize()
{
  File f(A2_TEST_DIR "/4096chunk.txt");
  REQUIRE_EQ((int64_t)4_k, f.size());
}

void FileTest::testMkdir()
{
  {
    std::string dir = A2_TEST_OUT_DIR "/aria2_FileTest_testMkdir/test";
    File d(dir);
    if (d.exists()) {
      REQUIRE(d.remove());
    }
    REQUIRE(!d.exists());

    REQUIRE(d.mkdirs());

    REQUIRE(d.exists());
    // this test fails because d.mkdir returns false when the directory is
    // already exists.
    REQUIRE(!d.mkdirs());
  }
  {
    std::string dir =
        A2_TEST_OUT_DIR "////aria2_FileTest_testMkdir////test2///";
    std::string nDir = A2_TEST_OUT_DIR "/aria2_FileTest_testMkdir/test2";
    File d(dir);
    File nd(nDir);
    if (d.exists()) {
      REQUIRE(d.remove());
    }
    REQUIRE(!nd.exists());

    REQUIRE(d.mkdirs());

    REQUIRE(nd.exists());
    // this test fails because d.mkdir returns false when the directory is
    // already exists.
    REQUIRE(!d.mkdirs());
  }
}

void FileTest::testGetDirname()
{
  {
    File f("/usr/lib");
    REQUIRE_EQ(std::string("/usr"), f.getDirname());
  }
  {
    File f("/usr/");
    REQUIRE_EQ(std::string("/usr"), f.getDirname());
  }
  {
    File f("usr");
    REQUIRE_EQ(std::string("."), f.getDirname());
  }
  {
    File f("/");
    REQUIRE_EQ(std::string("/"), f.getDirname());
  }
  {
    File f(".");
    REQUIRE_EQ(std::string("."), f.getDirname());
  }
  {
    File f("..");
    REQUIRE_EQ(std::string("."), f.getDirname());
  }
  {
    File f("");
    REQUIRE_EQ(std::string(""), f.getDirname());
  }
#ifdef __MINGW32__
  {
    File f("c:\\foo\\bar");
    REQUIRE_EQ(std::string("c:\\foo"), f.getDirname());
  }
#endif // __MINGW32__
}

void FileTest::testGetBasename()
{
  {
    File f("/usr/lib");
    REQUIRE_EQ(std::string("lib"), f.getBasename());
  }
  {
    File f("/usr/");
    REQUIRE_EQ(std::string(""), f.getBasename());
  }
  {
    File f("usr");
    REQUIRE_EQ(std::string("usr"), f.getBasename());
  }
  {
    File f("/");
    REQUIRE_EQ(std::string(""), f.getBasename());
  }
  {
    File f(".");
    REQUIRE_EQ(std::string("."), f.getBasename());
  }
  {
    File f("..");
    REQUIRE_EQ(std::string(".."), f.getBasename());
  }
  {
    File f("");
    REQUIRE_EQ(std::string(""), f.getBasename());
  }
#ifdef __MINGW32__
  {
    File f("c:\\foo\\bar");
    REQUIRE_EQ(std::string("bar"), f.getBasename());
  }
  {
    File f("c:\\foo\\");
    REQUIRE_EQ(std::string(""), f.getBasename());
  }
#endif // __MINGW32__
}

void FileTest::testRenameTo()
{
  std::string fname = A2_TEST_OUT_DIR "/aria2_FileTest_testRenameTo.txt";
  std::ofstream of(fname.c_str(), std::ios::binary);
  of.close();

  File f(fname);
  std::string fnameTo = A2_TEST_OUT_DIR "/aria2_FileTest_testRenameTo_dest.txt";
  REQUIRE(f.renameTo(fnameTo));
  REQUIRE(f.exists());
  REQUIRE(!File(fname).exists());
  REQUIRE_EQ(File(fnameTo).getBasename(), f.getBasename());

  // to see renameTo() work even when the destination file exists
  of.open(fname.c_str());
  of.close();

  REQUIRE(f.renameTo(fname));
}

void FileTest::testUtime()
{
  File f(A2_TEST_OUT_DIR "/aria2_FileTest_testUTime");
  createFile(f.getPath(), 0);

  time_t atime = (time_t)100000;
  time_t mtime = (time_t)200000;

  REQUIRE(f.utime(Time(atime), Time(mtime)));

  a2_struct_stat buf;
  REQUIRE(0 == a2stat(utf8ToWChar(f.getPath()).c_str(), &buf));
  REQUIRE_EQ((time_t)atime, (time_t)buf.st_atime);
  REQUIRE_EQ((time_t)mtime, f.getModifiedTime().getTimeFromEpoch());

  File notFound(A2_TEST_OUT_DIR "/aria2_FileTest_testUTime_notFound");
  notFound.remove();
  REQUIRE(!notFound.utime(Time(atime), Time(mtime)));
}

} // namespace aria2
