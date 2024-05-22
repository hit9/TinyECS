#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("fieldproxy-integeral/2", "[test integeral operators]") {
  World w;
  SETUP_INDEX;

  auto &a = w.NewArchetype<D>();
  auto e = a.NewEntity();
  auto d = a.NewEntity();

  e.Get<D>().x = 1;
  e.Get<D>().x += 1; // 2

  d.Get<D>().x = 1;
  d.Get<D>().x += 3; // 4

  REQUIRE(!e.Get<D>().x == false);
  REQUIRE(e.Get<D>().x < 3);
  REQUIRE(e.Get<D>().x < d.Get<D>().x);
  REQUIRE(e.Get<D>().x <= 2);
  REQUIRE(e.Get<D>().x <= d.Get<D>().x);
  REQUIRE(e.Get<D>().x > 1);
  REQUIRE(d.Get<D>().x > e.Get<D>().x);
  REQUIRE(d.Get<D>().x >= 4);
  REQUIRE(d.Get<D>().x >= e.Get<D>().x);

  e.Get<D>().x += d.Get<D>().x; // 6
  REQUIRE(e.Get<D>().x == 6);
}
