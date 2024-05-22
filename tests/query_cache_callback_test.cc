#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"
#include <unordered_set>

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("query_cache_callback", "[callback registeration and removal]") {
  World w;
  SETUP_INDEX;

  auto &a = w.NewArchetype<D>();
  auto eid = a.NewEntity();
  w.Get(eid).Get<D>().x = 1;

  Query<D> q(w);
  q.PreMatch().Where(index1 >= 1);

  { // open a new scope
    auto c = q.Cache();

    REQUIRE(w.NumCallbacks() == 2); // callback should registered
    REQUIRE(index1.NumCallbacks() == 1);
  }

 // callback should be removed
  REQUIRE(w.NumCallbacks() == 0);
  REQUIRE(index1.NumCallbacks() == 0);
}
