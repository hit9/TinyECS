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

  std::vector<EntityId> constructionOrders;

  auto eid1 = a1.DelayedNewEntity([&constructionOrders](EntityReference &e) {
    e.Construct<A>();
    constructionOrders.push_back(e.GetId());
  });
  auto eid2 = a2.DelayedNewEntity([&constructionOrders](EntityReference &e) {
    e.Construct<E>(378, "xyza");
    constructionOrders.push_back(e.GetId());
  });
  auto eid3 = a3.DelayedNewEntity([&constructionOrders](EntityReference &e) {
    e.Construct<E>(667, "xyz");
    e.Construct<A>();
    constructionOrders.push_back(e.GetId());
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

  // And the construction order should respect to the DelayedXXX calls.
  REQUIRE(constructionOrders == std::vector<EntityId>{eid1, eid2, eid3});

  // Queries should work now
  vec1.clear(), vec2.clear();
  q1.Collect(vec1), q2.Collect(vec2);
  REQUIRE(vec1 == std::vector<EntityReference>{w.Get(eid1), w.Get(eid3)});
  REQUIRE(vec2 == std::vector<EntityReference>{w.Get(eid2), w.Get(eid3)});

  // Queries with index should also work
  Query<E> q3(w, {index2 == "xyz"});
  std::vector<EntityReference> vec3;
    q3.PreMatch().Collect(vec3);
    REQUIRE(vec3 == std::vector<EntityReference>{w.Get(eid3)});
}

TEST_CASE("delayed/kill") {
  World w;
  SETUP_INDEX;
  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<E>();
  auto &a3 = w.NewArchetype<E, A>();
  auto e1 = a1.NewEntity();
  auto e2 = a2.NewEntity();
  auto e3 = a3.NewEntity();

  // Now each archetype contains an entity.
  REQUIRE(a1.NumEntities() == 1);
  REQUIRE(a2.NumEntities() == 1);
  REQUIRE(a3.NumEntities() == 1);

  // Now delay kill them.
  std::vector<EntityId> killOrders;

  Accessor cb = [&killOrders](EntityReference &ref) { killOrders.push_back(ref.GetId()); };
  e1.DelayedKill(cb);
  e2.DelayedKill(cb);
  e3.DelayedKill(cb);
  auto eid1 = e1.GetId();
  auto eid2 = e2.GetId();
  auto eid3 = e3.GetId();

  // Should not deleted yet.
  REQUIRE(e1.IsAlive());
  REQUIRE(e2.IsAlive());
  REQUIRE(e3.IsAlive());
  REQUIRE(w.Get(e1.GetId()).IsAlive());
  REQUIRE(w.Get(e2.GetId()).IsAlive());
  REQUIRE(w.Get(e3.GetId()).IsAlive());

  // Let's kill them
  w.ApplyDelayedKills();
  REQUIRE(a1.NumEntities() == 0);
  REQUIRE(a2.NumEntities() == 0);
  REQUIRE(a3.NumEntities() == 0);

  // Should all dead.
  REQUIRE(!e1.IsAlive());
  REQUIRE(!e2.IsAlive());
  REQUIRE(!e3.IsAlive());
  REQUIRE(!w.Get(e1.GetId()).IsAlive());
  REQUIRE(!w.Get(e2.GetId()).IsAlive());
  REQUIRE(!w.Get(e3.GetId()).IsAlive());

  // Kill order?
  REQUIRE(killOrders == std::vector<EntityId>{eid1, eid2, eid3});

  // Queries
  Query<A> q1(w);
  std::vector<EntityReference> vec1;
  q1.PreMatch().Collect(vec1);
  REQUIRE(vec1.empty());

  Query<E> q2(w);
  std::vector<EntityReference> vec2;
  q2.PreMatch().Collect(vec2);
  REQUIRE(vec2.empty());
}
