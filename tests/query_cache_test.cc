#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("query_cache", "[simple]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<A>();
  auto &a2 = w.NewArchetype<B>();
  auto &a3 = w.NewArchetype<A, D>();
  auto &a4 = w.NewArchetype<B, D>();

  auto e1 = a1.NewEntity();
  auto e2 = a2.NewEntity();
  auto e3 = a3.NewEntity();
  auto e4 = a4.NewEntity();

  w.Get(e1).Get<A>().x = -3;
  w.Get(e1).Get<A>().y = 18;

  w.Get(e2).Get<B>().s = "xyz";

  w.Get(e3).Get<A>().x = 4;
  w.Get(e3).Get<A>().y = 19;
  w.Get(e3).Get<D>().x = 1233;

  w.Get(e4).Get<B>().s = "xyz";
  w.Get(e4).Get<D>().x = 1222;

  Query<A> q1(w);                      // e1, e3
  Query<D> q2(w);                      // e3, e4
  Query<D> q3(w, {index1 <= 1222});    // e4
  Query<A, D> q4(w, {index1 <= 1222}); // {}

  q1.PreMatch(), q2.PreMatch(), q3.PreMatch(), q4.PreMatch();

  std::unordered_set<EntityId> m1, m2, m3, m4;
  q1.ForEach([&m1](EntityReference &e) { m1.insert(e.GetId()); });
  q2.ForEach([&m2](EntityReference &e) { m2.insert(e.GetId()); });
  q3.ForEach([&m3](EntityReference &e) { m3.insert(e.GetId()); });
  q4.ForEach([&m4](EntityReference &e) { m4.insert(e.GetId()); });
  REQUIRE(m1 == decltype(m1){e1, e3});
  REQUIRE(m2 == decltype(m2){e3, e4});
  REQUIRE(m3 == decltype(m3){e4});
  REQUIRE(m4 == decltype(m4){});

  auto cache1 = q1.Cache();
  auto cache2 = q2.Cache();
  auto cache3 = q3.Cache();
  auto cache4 = q4.Cache();

  std::unordered_set<EntityId> c1, c2, c3, c4;

  cache1.ForEach([&c1](EntityReference &e) { c1.insert(e.GetId()); });
  cache2.ForEach([&c2](EntityReference &e) { c2.insert(e.GetId()); });
  cache3.ForEach([&c3](EntityReference &e) { c3.insert(e.GetId()); });
  cache4.ForEach([&c4](EntityReference &e) { c4.insert(e.GetId()); });

  REQUIRE(m1 == c1);
  REQUIRE(m2 == c2);
  REQUIRE(m3 == c3);
  REQUIRE(m4 == c4);

  // kill e3;
  w.Kill(e3);

  std::unordered_set<EntityId> d1, // e1
      d2,                          // e4
      d3,                          // e4
      d4;                          // {}
  cache1.ForEach([&d1](EntityReference &e) { d1.insert(e.GetId()); });
  cache2.ForEach([&d2](EntityReference &e) { d2.insert(e.GetId()); });
  cache3.ForEach([&d3](EntityReference &e) { d3.insert(e.GetId()); });
  cache4.ForEach([&d4](EntityReference &e) { d4.insert(e.GetId()); });

  REQUIRE(d1 == decltype(d1){e1});
  REQUIRE(d2 == decltype(d2){e4});
  REQUIRE(d3 == decltype(d3){e4});
  REQUIRE(d4 == decltype(d4){});

  // add e5, matches q2, q3
  auto e5 = a4.NewEntity();

  std::unordered_set<EntityId> f1, // e1
      f2,                          // e4, e5
      f3,                          // e4, e5
      f4;                          // {}
  cache1.ForEach([&f1](EntityReference &e) { f1.insert(e.GetId()); });
  cache2.ForEach([&f2](EntityReference &e) { f2.insert(e.GetId()); });
  cache3.ForEach([&f3](EntityReference &e) { f3.insert(e.GetId()); });
  cache4.ForEach([&f4](EntityReference &e) { f4.insert(e.GetId()); });
  REQUIRE(f1 == decltype(f1){e1});
  REQUIRE(f2 == decltype(f2){e4, e5});
  REQUIRE(f3 == decltype(f3){e4, e5});
  REQUIRE(f4 == decltype(f4){});

  // update e4.x => 1223, creates e6
  w.Get(e4).Get<D>().x = 1223;
  auto e6 = a3.NewEntity(); // matches, q1,q2 q3,q4
  w.Get(e6).Get<D>().x = 1000;

  std::unordered_set<EntityId> g1, // e1
      g2,                          // e4, e5
      g3,                          // e5, e6
      g4;                          // e6
  cache1.ForEach([&g1](EntityReference &e) { g1.insert(e.GetId()); });
  cache2.ForEach([&g2](EntityReference &e) { g2.insert(e.GetId()); });
  cache3.ForEach([&g3](EntityReference &e) { g3.insert(e.GetId()); });
  cache4.ForEach([&g4](EntityReference &e) { g4.insert(e.GetId()); });
  REQUIRE(g1 == decltype(g1){e1, e6});
  REQUIRE(g2 == decltype(g2){e4, e5, e6});
  REQUIRE(g3 == decltype(g3){e6, e5});
  REQUIRE(g4 == decltype(g4){e6});
}
