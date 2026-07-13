#include "SequentialPicker.h"

#include "a2doctest.h"

#include "a2functional.h"

namespace aria2 {

class SequentialPickerTest {


public:
  void testPick();
};

A2_TEST(SequentialPickerTest, testPick)

void SequentialPickerTest::testPick()
{
  SequentialPicker<int> picker;

  REQUIRE(!picker.isPicked());
  REQUIRE(!picker.hasNext());
  REQUIRE_EQ((size_t)0, picker.countEntryInQueue());

  picker.pushEntry(make_unique<int>(1));
  picker.pushEntry(make_unique<int>(2));

  REQUIRE(picker.hasNext());
  REQUIRE_EQ((size_t)2, picker.countEntryInQueue());

  picker.pickNext();

  REQUIRE(picker.isPicked());
  REQUIRE_EQ(1, *picker.getPickedEntry());

  picker.dropPickedEntry();

  REQUIRE(!picker.isPicked());
  REQUIRE(picker.hasNext());

  picker.pickNext();

  REQUIRE_EQ(2, *picker.getPickedEntry());
  REQUIRE(!picker.hasNext());
}

} // namespace aria2
