
#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("index/1", "[unbound index: component shouldn't write index]") {
  // index is not bind to a world
  D d;
  REQUIRE(index1.Size() == 0);
}

TEST_CASE("index/2", "[unbound field: component shouldn't write index]") {
  World w;
  SETUP_INDEX;
  struct X {
    FieldProxy<int, decltype(index1)> x = 0;
  }; // X's constructor not bind index1;
  // And there's no entity creation
  X x;
  REQUIRE(index1.Size() == 0);
}

TEST_CASE("index/3", "[unbound index: create entity shouldn't write index]") {
  World w;
  auto &a = w.NewArchetype<D>();
  a.NewEntity();
  REQUIRE(index1.Size() == 0);
}

TEST_CASE("index/4", "[unbound fined: write entity should throw]") {
  struct X {
    FieldProxy<int, decltype(index1)> x = 0;
  }; // X's constructor not bind index1;
  World w;
  SETUP_INDEX;
  auto &a = w.NewArchetype<X>();
  auto e = a.NewEntity();
  REQUIRE_THROWS_AS(w.Get(e).Get<X>().x = 1, std::runtime_error);
}
