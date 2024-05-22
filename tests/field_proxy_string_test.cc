#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("fieldproxy-string/1", "[test string operators]") {
  World w;
  SETUP_INDEX;

  auto &a = w.NewArchetype<E>();
  auto eid = a.NewEntity();
  w.Get(eid).Get<E>().z += "abc";
  REQUIRE(w.Get(eid).Get<E>().z == "abcabc");
  REQUIRE(w.Get(eid).Get<E>().z.GetValue().size() == 6);
  w.Get(eid).Get<E>().z = std::string_view("zh");
  REQUIRE(w.Get(eid).Get<E>().z == "zh");
  w.Get(eid).Get<E>().z += std::string_view("zh");
  REQUIRE(w.Get(eid).Get<E>().z == "zhzh");
  w.Get(eid).Get<E>().z += std::string("zh");
  REQUIRE(w.Get(eid).Get<E>().z == "zhzhzh");
}
