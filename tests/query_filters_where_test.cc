#include <catch2/catch_test_macros.hpp>
#include <unordered_set>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("query_filters_where/1", "[simple]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<D, E>();
  auto &a2 = w.NewArchetype<D, E>();
  auto &a3 = w.NewArchetype<D, F>();

  auto e1 = a1.NewEntity();
  auto e2 = a2.NewEntity();
  auto e3 = a3.NewEntity();

  w.Get(e1).Get<D>().x = 0;
  w.Get(e1).Get<E>().z = "xyz";

  w.Get(e2).Get<D>().x = 1;
  w.Get(e2).Get<E>().z = "xyz";

  w.Get(e3).Get<D>().x = 3;
  w.Get(e3).Get<F>().status = Status::S2;

  Query<D, E> q(w, {index1 >= 1});
  q.PreMatch();
  q.Where(Filters{index2 == "xyz"}).Where(index1 < 2);

  std::unordered_set<EntityId> z1;
  q.ForEach([&z1](EntityReference &e) { z1.insert(e.GetId()); });
  REQUIRE(z1 == decltype(z1){e2});

  q.ClearFilters();
  q.Where(Filters{index2 == "xyz"}).Where(index1 < 2);
  z1.clear();
  q.ForEach([&z1](EntityReference &e) { z1.insert(e.GetId()); });
  REQUIRE(z1 == decltype(z1){e1, e2});
}
