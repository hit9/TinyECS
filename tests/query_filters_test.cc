#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("query_filters/1", "[simple]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<D>();

  auto e1 = a1.NewEntity();
  e1.Get<D>().x = 1;
  Query<D> q1(w, {index1 == 1});
  q1.PreMatch();

  int cnt = 0;
  Accessor cb1 = [&](EntityReference &e) {
    REQUIRE(e.GetId() == e1.GetId());
    REQUIRE(e.Get<D>().x == 1);
    cnt++;
  };

  q1.ForEach(cb1);
  REQUIRE(cnt == 1);

  REQUIRE(e1.IsAlive());

  e1.Kill();
  cnt = 0;
  q1.ForEach(cb1);
  REQUIRE(cnt == 0);
  REQUIRE(!e1.IsAlive());
}

TEST_CASE("query_filters/2", "[multiple index]") {
  World w;
  SETUP_INDEX;

  auto &a1 = w.NewArchetype<D>();
  auto &a2 = w.NewArchetype<E>();
  auto &a3 = w.NewArchetype<D, E>();

  auto e1 = a1.NewEntity();
  auto e2 = a1.NewEntity();
  auto e3 = a2.NewEntity();
  auto e4 = a2.NewEntity();
  auto e5 = a3.NewEntity();

  e1.Get<D>().x = 3;

  e2.Get<D>().x = 9;

  e3.Get<E>().x = 3;
  e3.Get<E>().z = std::string("edf");

  e4.Get<E>().x = 12;

  e5.Get<D>().x = 3;
  e5.Get<E>().x = 19;
  e5.Get<E>().z = std::string("edf");

  //////// query x==3
  Query<D> q1(w, {index1 == 3});
  q1.PreMatch();
  std::vector<EntityId> m1;
  q1.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<D>().x == 3);
    m1.push_back(e.GetId());
  });
  REQUIRE(m1 == decltype(m1){e1.GetId(), e5.GetId()});

  //////// query z==edf
  Query<E> q2(w, {index2 == "edf"});
  q2.PreMatch();
  std::vector<EntityId> m2;
  q2.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<E>().z == "edf");
    m2.push_back(e.GetId());
  });
  REQUIRE(m2 == decltype(m2){e3.GetId(), e5.GetId()});

  //////// query z==edf && x==3
  Query<D, E> q3(w, {index1 == 3, index2 == "edf"});
  q3.PreMatch();
  std::vector<EntityId> m3;
  q3.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<D>().x == 3);
    REQUIRE(e.Get<E>().z == "edf");
    m3.push_back(e.GetId());
  });
  REQUIRE(m3 == decltype(m3){e5.GetId()});

  //////// update e5.x and recheck query x==3
  e5.Get<D>().x = 1;
  std::vector<EntityId> m4;
  Query<D> q4(w, {index1 == 3});
  q4.PreMatch();
  q4.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<D>().x == 3);
    m4.push_back(e.GetId());
  });
  REQUIRE(m4 == decltype(m4){e1.GetId()});

  //////// update e5.z and recheck query z=="edf"
  e5.Get<E>().z += "hellowold";
  std::vector<EntityId> m5;
  Query<E> q5(w, {index2 == "edf"});
  q5.PreMatch();
  q5.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<E>().z == "edf");
    m5.push_back(e.GetId());
  });
  REQUIRE(m5 == decltype(m5){e3.GetId()});

  //////// query x < 17
  std::vector<EntityId> m6;
  Query<E> q6(w, {index3 < 17});
  q6.PreMatch();
  q6.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<E>().x < 17);
    m6.push_back(e.GetId());
  });
  REQUIRE(m6 == decltype(m6){e3.GetId(), e4.GetId()});

  //////// query E.x >=12 && z != abc
  std::vector<EntityId> m7;
  Query<E> q7(w, {index3 >= 12, index2 != "abc"});
  q7.PreMatch();
  q7.ForEach([&](EntityReference &e) {
    REQUIRE(e.IsAlive());
    REQUIRE(e.Get<E>().x >= 12);
    REQUIRE(e.Get<E>().z != "abc");
    m7.push_back(e.GetId());
  });
  REQUIRE(m7 == decltype(m7){e5.GetId()});
}
