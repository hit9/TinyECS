#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "TinyECS.h"
#include <vector>

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("cache_filters", "[simple]")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<D>();
	auto& a2 = w.NewArchetype<E>();
	auto& a3 = w.NewArchetype<F>();
	auto& a4 = w.NewArchetype<D, E>();
	auto& a5 = w.NewArchetype<D, E, F>();

	auto e1 = a1.NewEntity();
	auto e2 = a2.NewEntity();
	auto e3 = a3.NewEntity();
	auto e4 = a4.NewEntity();
	auto e5 = a5.NewEntity();

	e1.Get<D>().x = 3;

	e2.Get<E>().x = 1;
	e2.Get<E>().z = "abc";

	e3.Get<F>().status = Status::S2;

	e4.Get<D>().x = 7;
	e4.Get<E>().x = 1;
	e4.Get<E>().z = "abc";

	e5.Get<D>().x = 4;
	e5.Get<E>().x = 1;
	e5.Get<E>().z = "abc";
	e5.Get<F>().status = Status::S2;

	Query<D>	q1(w, { index1 <= 6 });						  // query D.x <= 6
	Query<E>	q2(w, { index2 == "abc" });					  // query E.z == "abc"
	Query<D, E> q3(w, { index3 < 10, index1 > 3 });			  // query D.x >=3 && E.x < 10
	Query<D, F> q4(w, { index1 <= 7, index5 == Status::S2 }); // query D.x <= 7 && F.status == S2 //

	q1.PreMatch(), q2.PreMatch(), q3.PreMatch(), q4.PreMatch();

	std::vector<EntityId> m1, // {e1, e5}
		m2,					  // {e2, e4, e5}
		m3,					  // {e4, e5}
		m4;					  // {e5}
	q1.ForEach([&m1](EntityReference& e) { m1.push_back(e.GetId()); });
	q2.ForEach([&m2](EntityReference& e) { m2.push_back(e.GetId()); });
	q3.ForEach([&m3](EntityReference& e) { m3.push_back(e.GetId()); });
	q4.ForEach([&m4](EntityReference& e) { m4.push_back(e.GetId()); });

	REQUIRE(m1 == decltype(m1){ e1.GetId(), e5.GetId() });
	REQUIRE(m2 == decltype(m2){ e2.GetId(), e4.GetId(), e5.GetId() });
	REQUIRE(m3 == decltype(m3){ e4.GetId(), e5.GetId() });
	REQUIRE(m4 == decltype(m4){ e5.GetId() });

	auto c1 = q1.Cache(), c2 = q2.Cache(), c3 = q3.Cache(), c4 = q4.Cache();

	std::vector<EntityId> z1, // {e1, e5}
		z2,					  // {e2, e4, e5}
		z3,					  // {e4, e5}
		z4;					  // {e5}
	c1.ForEach([&z1](EntityReference& e) { z1.push_back(e.GetId()); });
	c2.ForEach([&z2](EntityReference& e) { z2.push_back(e.GetId()); });
	c3.ForEach([&z3](EntityReference& e) { z3.push_back(e.GetId()); });
	c4.ForEach([&z4](EntityReference& e) { z4.push_back(e.GetId()); });

	REQUIRE(z1 == m1);
	REQUIRE(z2 == m2);
	REQUIRE(z3 == m3);
	REQUIRE(z4 == m4);

	// update e5 D.x = 9
	// should removed from q1, and q4
	// {e1}, {e2,e4,e5}, {e4,e5}, {}
	e5.Get<D>().x = 9;
	z1.clear(), z2.clear(), z3.clear(), z4.clear();
	c1.ForEach([&z1](EntityReference& e) { z1.push_back(e.GetId()); });
	c2.ForEach([&z2](EntityReference& e) { z2.push_back(e.GetId()); });
	c3.ForEach([&z3](EntityReference& e) { z3.push_back(e.GetId()); });
	c4.ForEach([&z4](EntityReference& e) { z4.push_back(e.GetId()); });
	REQUIRE(z1 == decltype(z1){ e1.GetId() });
	REQUIRE(z2 == decltype(z2){ e2.GetId(), e4.GetId(), e5.GetId() });
	REQUIRE(z3 == decltype(z3){ e4.GetId(), e5.GetId() });
	REQUIRE(z4 == decltype(z4){});

	// add e6 D.x=5, F.status = S2, E.z = "xyz", E.x = 11, matches c1,c4
	// and updates  e4 D.x =1, E.z = "xyz", should removed from c2, c3, should add to c1
	// c1 c2 c3 c4 expects to {e1,e4,e6},{e2,e5}, {e5}, {e6}
	auto e6 = a5.NewEntity();
	e6.Get<D>().x = 5;
	e6.Get<E>().x = 11;
	e6.Get<E>().z = "xyz";
	e6.Get<F>().status = Status::S2;
	e4.Get<D>().x = 1;
	e4.Get<E>().z = "xyz";

	z1.clear(), z2.clear(), z3.clear(), z4.clear();
	c1.ForEach([&z1](EntityReference& e) { z1.push_back(e.GetId()); });
	c2.ForEach([&z2](EntityReference& e) { z2.push_back(e.GetId()); });
	c3.ForEach([&z3](EntityReference& e) { z3.push_back(e.GetId()); });
	c4.ForEach([&z4](EntityReference& e) { z4.push_back(e.GetId()); });
	REQUIRE(z1 == decltype(z1){ e1.GetId(), e4.GetId(), e6.GetId() });
	REQUIRE(z2 == decltype(z2){ e2.GetId(), e5.GetId() });
	REQUIRE(z3 == decltype(z3){ e5.GetId() });
	REQUIRE(z4 == decltype(z4){ e6.GetId() });
}
