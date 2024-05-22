#include <catch2/catch_test_macros.hpp>
#include <unordered_set>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("query_simple", "[simple]") {
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
  REQUIRE(m1 == decltype(m1){e1, e2, e3});

  Query<B> q2(w);
  q2.PreMatch();
  std::unordered_set<EntityId> m2;
  q2.ForEach([&](EntityReference &e) { m2.insert(e.GetId()); });
  REQUIRE(m2 == decltype(m2){e1, e2, e4});

  QueryAny<C> q3(w);
  q3.PreMatch();
  std::unordered_set<EntityId> m3;
  q3.ForEach([&](EntityReference &e) { m3.insert(e.GetId()); });
  REQUIRE(m3 == decltype(m3){e3, e4});

  QueryNone<A, C> q4(w);
  q4.PreMatch();
  std::unordered_set<EntityId> m4;
  q4.ForEach([&](EntityReference &e) { m4.insert(e.GetId()); });
  REQUIRE(m4.empty());

  QueryAny<> q5(w);
  q5.PreMatch();
  std::unordered_set<EntityId> m5;
  q5.ForEach([&](EntityReference &e) { m5.insert(e.GetId()); });
  REQUIRE(m5 == decltype(m5){e1, e2, e3, e4});

  w.Kill(e1);
  Query<A> q6(w);
  q6.PreMatch();
  std::unordered_set<EntityId> m6;
  q6.ForEach([&](EntityReference &e) { m6.insert(e.GetId()); });
  REQUIRE(m6 == decltype(m1){e2, e3});
}
