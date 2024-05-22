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
  auto e1 = a1.NewEntity();
  auto e2 = a1.NewEntity();
  REQUIRE(w.Get(e1).IsAlive());
  REQUIRE(w.Get(e1).GetId() == e1);
  REQUIRE(w.Get(e2).GetId() == e2);
}

TEST_CASE("world/3", "[Get with callback]") {
  World w;
  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<B>();
  auto e1 = a1.NewEntity();
  auto e2 = a1.NewEntity();
  bool called = false;
  w.Get(e1, [&e1, &called](EntityReference &ref) {
    REQUIRE(ref.GetId() == e1);
    called = true;
  });
  REQUIRE(called);
  w.Get(e2, [&e2](EntityReference &ref) { REQUIRE(ref.GetId() == e2); });
}

TEST_CASE("world/4", "[UncheckedGet]") {
  World w;
  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<B>();
  auto e1 = a1.NewEntity();
  auto e2 = a1.NewEntity();
  REQUIRE(w.UncheckedGet(e1).GetId() == e1);
  REQUIRE(w.UncheckedGet(e2).GetId() == e2);
}

TEST_CASE("world/5", "[UncheckedGet with callback]") {
  World w;
  auto &a1 = w.NewArchetype<A>();
  auto e1 = a1.NewEntity();
  bool called = false;
  w.UncheckedGet(e1, [&e1, &called](EntityReference &ref) {
    REQUIRE(ref.GetId() == e1);
    called = true;
  });
  REQUIRE(called);
}
