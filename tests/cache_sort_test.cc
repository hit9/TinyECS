#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"
#include <vector>

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("cache_sort", "[custom compare]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<D>();
  auto &a2 = w.NewArchetype<D, F>();
  auto &a3 = w.NewArchetype<D, E>();

  auto e1 = a1.NewEntity([](EntityReference &e) { e.Get<D>().x = 8; });
  auto e2 = a1.NewEntity([](EntityReference &e) { e.Get<D>().x = 9; });
  auto e3 = a2.NewEntity([](EntityReference &e) { e.Get<D>().x = 3; });
  auto e4 = a3.NewEntity([](EntityReference &e) { e.Get<D>().x = 1; });

  Query<D> q(w);
  auto cmp = [&w](const EntityId a, const EntityId b) { return w.Get(a).Get<D>().x < w.Get(b).Get<D>().x; };
  auto c = q.PreMatch().Cache<decltype(cmp)>(cmp);
  std::vector<EntityId> z;
  c.ForEach([&z](EntityReference &ref) { z.push_back(ref.GetId()); });
  REQUIRE(z == decltype(z){e4, e3, e1, e2});
}
