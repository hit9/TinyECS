#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("cemetery/1", "[simple]") {
  __internal::Cemetery cemetery;
  REQUIRE(!cemetery.Contains(3777));
  REQUIRE(cemetery.Size() == 0);
  cemetery.Add(3777);
  cemetery.Add(273);
  REQUIRE(cemetery.Contains(3777));
  REQUIRE(cemetery.Contains(273));
  REQUIRE(cemetery.Size() == 2);
  REQUIRE(cemetery.Pop() == 3777); // FIFO
  REQUIRE(!cemetery.Contains(3777));
  REQUIRE(cemetery.Pop() == 273);
  REQUIRE(!cemetery.Contains(273));
  REQUIRE(cemetery.Size() == 0);
}

TEST_CASE("cemetery/2", "[allocates new block]") {
  __internal::Cemetery cemetery;
  EntityShortId e = 0;
  for (int i = 0; i < __internal::Cemetery::NumRowsPerBlock; i++)
    cemetery.Add(e++);
  REQUIRE(cemetery.Size() == __internal::Cemetery::NumRowsPerBlock);
  for (int i = 0; i < __internal::Cemetery::NumRowsPerBlock; i++)
    cemetery.Add(e++);
  REQUIRE(cemetery.Size() == 2 * __internal::Cemetery::NumRowsPerBlock);
  EntityShortId e1 = 0x7ffff;
  cemetery.Add(e1);
  REQUIRE(cemetery.Contains(e1));
  e = 0;
  for (int i = cemetery.Size()-1; i; --i, e++) {
    REQUIRE(cemetery.Contains(e));
    REQUIRE(cemetery.Pop() == e);
    REQUIRE(!cemetery.Contains(e));
  }
  REQUIRE(cemetery.Pop() == e1);
}
