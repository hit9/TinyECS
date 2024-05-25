#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("delayed/new") {
  World w;
  SETUP_INDEX;
  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<E>();
  auto &a3 = w.NewArchetype<E, A>();
  REQUIRE(a1.NumEntities() == 0);
  REQUIRE(a2.NumEntities() == 0);
  REQUIRE(a3.NumEntities() == 0);

  auto eid1 = a1.DelayedNewEntity();
  auto eid2 = a2.DelayedNewEntity([](EntityReference &e) { e.Construct<E>(378, "xyza"); });
  auto eid3 = a3.DelayedNewEntity([](EntityReference &e) {
    e.Construct<E>(667, "xyz");
    e.Construct<A>();
  });

  // still no entities alive before apply
  REQUIRE(a1.NumEntities() == 0);
  REQUIRE(a2.NumEntities() == 0);
  REQUIRE(a3.NumEntities() == 0);

  // We can't query any of them.
  REQUIRE(!w.IsAlive(eid1));
  REQUIRE(!w.Get(eid1).IsAlive());
  REQUIRE(!w.IsAlive(eid2));
  REQUIRE(!w.Get(eid2).IsAlive());
  REQUIRE(!w.IsAlive(eid3));
  REQUIRE(!w.Get(eid3).IsAlive());

  Query<A> q1(w);
  std::vector<EntityReference> vec1;
  q1.PreMatch().Collect(vec1);
  REQUIRE(vec1.empty());

  Query<E> q2(w);
  std::vector<EntityReference> vec2;
  q2.PreMatch().Collect(vec2);
  REQUIRE(vec2.empty());

  // Now let's apply
  w.ApplyDelayedNewEntities();
  REQUIRE(a1.NumEntities() == 1);
  REQUIRE(a2.NumEntities() == 1);
  REQUIRE(a3.NumEntities() == 1);

  // All entities becomes alive.
  REQUIRE(w.Get(eid1).IsAlive());
  REQUIRE(w.Get(eid2).IsAlive());
  REQUIRE(w.Get(eid3).IsAlive());

  // How about the initial data of each?
  REQUIRE(w.Get(eid1).Get<A>().x == 0);
  REQUIRE(w.Get(eid1).Get<A>().y == 1);
  REQUIRE(w.Get(eid2).Get<E>().x == 378);
  REQUIRE(w.Get(eid2).Get<E>().z == "xyza");
  REQUIRE(w.Get(eid3).Get<A>().x == 0);
  REQUIRE(w.Get(eid3).Get<A>().y == 1);
  REQUIRE(w.Get(eid3).Get<E>().x == 667);
  REQUIRE(w.Get(eid3).Get<E>().z == "xyz");
}
