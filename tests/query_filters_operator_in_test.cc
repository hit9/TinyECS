
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("query_filters_operator_in")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<E>();
	auto& a2 = w.NewArchetype<F>();
	auto& a3 = w.NewArchetype<H>();
	auto& a4 = w.NewArchetype<E, F>();
	auto& a5 = w.NewArchetype<E, F, H>();

	auto e1 = a1.NewEntity();
	auto e2 = a2.NewEntity();
	auto e3 = a3.NewEntity();
	auto e4 = a4.NewEntity();
	auto e5 = a5.NewEntity();

	e1.Get<E>().z = "efg";
	e2.Get<F>().status = Status::S2;
	e3.Get<H>().h = "xyz";

	e4.Get<E>().x = 1;
	e4.Get<E>().z = "efg";
	e4.Get<F>().status = Status::S2;

	e5.Get<E>().x = 2;
	e5.Get<E>().z = "efg1111";
	e5.Get<F>().status = Status::S3;
	e5.Get<H>().h = "xyz";

	// query E.z in {efg}
	Query<E> q1(w, { index2.In({ "efg" }) });
	q1.PreMatch();
	std::vector<EntityId> m1;
	q1.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		REQUIRE(e.Get<E>().z == "efg");
		m1.push_back(e.GetId());
	});
	REQUIRE(m1 == decltype(m1){ e1.GetId(), e4.GetId() });

	// query  F.status in {S2}
	Query<F> q2(w, { index5.In({ Status::S2 }) });
	q2.PreMatch();
	std::vector<EntityId> m2;
	q2.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		REQUIRE(e.Get<F>().status == Status::S2);
		m2.push_back(e.GetId());
	});
	REQUIRE(m2 == decltype(m2){ e2.GetId(), e4.GetId() });

	// query E.z in {efg} && F.status in {S2}
	Query<F> q3(w, { index2.In({ "efg" }), index5.In({ Status::S2 }) });
	q3.PreMatch();
	std::vector<EntityId> m3;
	q3.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		REQUIRE(e.Get<E>().z == "efg");
		REQUIRE(e.Get<F>().status == Status::S2);
		m3.push_back(e.GetId());
	});
	REQUIRE(m3 == decltype(m3){ e4.GetId() });
}
