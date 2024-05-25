#include <catch2/catch_test_macros.hpp>
#include <unordered_set>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("query_simple/1", "[simple]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<A, B>();
  auto &a2 = w.NewArchetype<A, C>();
  auto &a3 = w.NewArchetype<B, C>();

  auto e1 = a1.NewEntity();
  auto e2 = a1.NewEntity();
  auto e3 = a2.NewEntity();
  auto e4 = a3.NewEntity();

  Query<A> q1(w);
  q1.PreMatch();
  std::unordered_set<EntityId> m1;
  q1.ForEach([&](EntityReference &e) { m1.insert(e.GetId()); });
  REQUIRE(m1 == decltype(m1){e1.GetId(), e2.GetId(), e3.GetId()});

  Query<B> q2(w);
  q2.PreMatch();
  std::unordered_set<EntityId> m2;
  q2.ForEach([&](EntityReference &e) { m2.insert(e.GetId()); });
  REQUIRE(m2 == decltype(m2){e1.GetId(), e2.GetId(), e4.GetId()});

  QueryAny<C> q3(w);
  q3.PreMatch();
  std::unordered_set<EntityId> m3;
  q3.ForEach([&](EntityReference &e) { m3.insert(e.GetId()); });
  REQUIRE(m3 == decltype(m3){e3.GetId(), e4.GetId()});

  QueryNone<A, C> q4(w);
  q4.PreMatch();
  std::unordered_set<EntityId> m4;
  q4.ForEach([&](EntityReference &e) { m4.insert(e.GetId()); });
  REQUIRE(m4.empty());

  QueryAny<> q5(w);
  q5.PreMatch();
  std::unordered_set<EntityId> m5;
  q5.ForEach([&](EntityReference &e) { m5.insert(e.GetId()); });
  REQUIRE(m5 == decltype(m5){e1.GetId(), e2.GetId(), e3.GetId(), e4.GetId()});

  w.Kill(e1.GetId());
  Query<A> q6(w);
  q6.PreMatch();
  std::unordered_set<EntityId> m6;
  q6.ForEach([&](EntityReference &e) { m6.insert(e.GetId()); });
  REQUIRE(m6 == decltype(m1){e2.GetId(), e3.GetId()});
}

TEST_CASE("query_simple/2", "[collect]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<A, D>();
  auto &a2 = w.NewArchetype<D, E>();

  auto e1 = a1.NewEntity();
  auto e2 = a1.NewEntity();
  auto e3 = a2.NewEntity();
  auto e4 = a2.NewEntity();

  e1.Get<A>().x = 3;
  e1.Get<D>().x = 3;

  e2.Get<D>().x = 44;

  e3.Get<D>().x = 32;
  e3.Get<E>().z = "xyz";
  e4.Get<D>().x = 99;

  Query<D> q(w);
  std::vector<EntityReference> vec;
  q.PreMatch().Where(index1 >= 4).Collect(vec);
  REQUIRE(vec == decltype(vec){e2, e3, e4});

  Query<D> q1(w);
  std::vector<EntityReference> vec1;
  q.PreMatch().Where(index1 >= 4).CollectUntil(vec1, [&](EntityReference &e) {
    if (e.GetId() == e4.GetId()) return true; // skip e4
    return false;
  });
  REQUIRE(vec1 == decltype(vec){e2, e3});
}
