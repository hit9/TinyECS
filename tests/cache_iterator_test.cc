#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("cache_iterator") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<D>();
  auto &a2 = w.NewArchetype<D, F>();

  auto e1 = a1.NewEntity([](EntityReference &e) { e.Get<D>().x = 3; });

  auto e2 = a2.NewEntity([](EntityReference &e) {
    e.Get<D>().x = 9;
    e.Get<F>().status = Status::S3;
  });

  auto e3 = a2.NewEntity([](EntityReference &e) {
    e.Get<D>().x = 18;
    e.Get<F>().status = Status::S2;
  });

  Query<D> q(w, {index1 >= 3, index1 <= 18});
  auto c = q.PreMatch().Cache();
  for (auto it = c.begin(); it != c.end(); ++it) {
    auto &e = it->second;
    REQUIRE(e.Get<D>().x >= 3);
    REQUIRE(e.Get<D>().x <= 18);
  }
}
