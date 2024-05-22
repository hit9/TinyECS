
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("archetype/1", "[simple]") {
  World w;
  auto &a = w.NewArchetype<A, B>();
  REQUIRE(a.GetId() == 0);
  REQUIRE(a.BlockSize() == 2 * MaxNumEntitiesPerBlock * std::max(sizeof(A), sizeof(B)));

  auto eid = a.NewEntity();

  REQUIRE(__internal::unpack_x(eid) == a.GetId());
  REQUIRE(__internal::unpack_y(eid) == 0);

  REQUIRE(w.IsAlive(eid));
  REQUIRE(!w.IsAlive(12301));

  int cnt = 0;
  Accessor cb = [&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.GetId() == eid);
    REQUIRE(e.GetArchetypeId() == a.GetId());
    REQUIRE(e.Get<A>().x == 0);
    REQUIRE(e.Get<A>().y == 1);
    REQUIRE(e.Get<B>().s == "abc");
    cnt++;
  };

  a.ForEach(cb);
  REQUIRE(cnt == 1);
  REQUIRE(cnt == a.NumEntities());

  w.Get(eid).Get<A>().x = 3;
  REQUIRE(w.Get(eid).Get<A>().x == 3); // reget
  Accessor cb1 = [&](EntityReference &e) {
    REQUIRE(e.Get<A>().x == 3);
    return false;
  };
  a.ForEach(cb1);

  REQUIRE(w.Get(eid).IsAlive());
  w.Kill(eid);
  REQUIRE(!w.IsAlive(eid));
  REQUIRE(!w.Get(eid).IsAlive());
  cnt = 0;
  a.ForEach(cb);
  REQUIRE(cnt == 0);
  REQUIRE(cnt == a.NumEntities());

  REQUIRE(a.NewEntity() == eid); // recycle
  w.Get(eid).Get<A>().x = 0;     // back to default value
}

TEST_CASE("archetype/2", "[allocate new block]") {
  World w;
  auto &a = w.NewArchetype<A, B>();
  for (int i = 0; i < MaxNumEntitiesPerBlock; i++)
    a.NewEntity();
  REQUIRE(a.NumBlocks() == 1);
  REQUIRE(MaxNumEntitiesPerBlock == a.NumEntities());
  a.NewEntity();
  REQUIRE(a.NumBlocks() == 2);
  REQUIRE(MaxNumEntitiesPerBlock + 1 == a.NumEntities());
}

TEST_CASE("archetype/3", "[constructors and desctructors]") {
  World w;
  auto &a = w.NewArchetype<A, K>();
  auto eid = a.NewEntity();
  REQUIRE(w.Get(eid).Get<K>().a == 1); // constructor called
  REQUIRE(w.Get(eid).Get<K>().b == 3); // constructor called
  kDescructorCalled = false;
  w.Kill(eid);
  REQUIRE(kDescructorCalled); // desctructor called
}

TEST_CASE("archetype/4", "[constructors and desctructors index bind]") {
  World w;
  SETUP_INDEX;
  auto &a = w.NewArchetype<D, F>();
  auto eid = a.NewEntity();
  REQUIRE(w.Get(eid).Get<D>().x == 0);
  REQUIRE(w.Get(eid).Get<F>().status == Status::S1);
  REQUIRE(w.Get(eid).Get<D>().x.IsBind());
  REQUIRE(w.Get(eid).Get<F>().status.IsBind());
}

TEST_CASE("archetype/5", "[NewEntity inplace]") {
  World w;
  SETUP_INDEX;
  auto &a = w.NewArchetype<D, F>();
  EntityReference e;
  auto eid = a.NewEntity(e);
  REQUIRE(e.GetId() == eid);
  REQUIRE(e.Get<D>().x == 0);
  REQUIRE(e.Get<F>().status == Status::S1);
  e.Get<D>().x += 999;
  REQUIRE(w.Get(eid).Get<D>().x == 999);
}

TEST_CASE("archetype/6", "[test get unknown column]") {
  World w;
  SETUP_INDEX;
  auto &a = w.NewArchetype<A>();
  auto eid = a.NewEntity();
  REQUIRE_THROWS_AS(w.Get(eid).Get<B>(), std::runtime_error);
}

TEST_CASE("archetype/7", "[NewEntity with callback 1]") {
  World w;
  SETUP_INDEX;
  auto &a = w.NewArchetype<D, F>();
  auto eid = a.NewEntity([](EntityReference &ref) { ref.Get<D>().x = 3333; });
  auto e = w.Get(eid);
  REQUIRE(e.GetId() == eid);
  REQUIRE(e.Get<D>().x == 3333);
}

TEST_CASE("archetype/7", "[NewEntity with callback 2]") {
  World w;
  SETUP_INDEX;
  auto &a = w.NewArchetype<D, F>();
  auto cb = [](EntityReference &ref) { ref.Get<D>().x = 3333; };
  auto eid = a.NewEntity(cb);
  auto e = w.Get(eid);
  REQUIRE(e.GetId() == eid);
  REQUIRE(e.Get<D>().x == 3333);
}
