#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("world/1", "[Get Not found]") {
  World w;
  auto &a = w.NewArchetype<A>();
  REQUIRE(!w.Get(0).IsAlive());
}

TEST_CASE("world/2", "[Get]") {
  World w;
  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<B>();
  auto eid1 = a1.NewEntity().GetId();
  auto eid2 = a2.NewEntity().GetId();
  REQUIRE(w.Get(eid1).IsAlive());
  REQUIRE(w.Get(eid1).GetId() == eid1);
  REQUIRE(w.Get(eid2).GetId() == eid2);
}

TEST_CASE("world/3", "[UncheckedGet]") {
  World w;
  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<B>();
  auto eid1 = a1.NewEntity().GetId();
  auto eid2 = a1.NewEntity().GetId();
  REQUIRE(w.UncheckedGet(eid1).GetId() == eid1);
  REQUIRE(w.UncheckedGet(eid2).GetId() == eid2);
}

TEST_CASE("world/4", "[IsAlive & Kill]") {
  World w;
  auto &a = w.NewArchetype<A>();
  REQUIRE(!w.IsAlive(0));
  auto e = a.NewEntity();
  auto eid = e.GetId();
  REQUIRE(w.IsAlive(eid));
  w.Kill(eid);
  REQUIRE(!w.IsAlive(eid));
}
