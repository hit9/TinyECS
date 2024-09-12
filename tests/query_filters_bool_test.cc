#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("query_filters_bool")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<G>();
	auto  e1 = a1.NewEntity();
	auto  e2 = a1.NewEntity();

	e1.Get<G>().isX = false;
	e2.Get<G>().isX = true;

	Query<G> q1(w, { index6 == true });
	q1.PreMatch();
	std::vector<EntityId> m1;
	q1.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		REQUIRE(e.Get<G>().isX);
		m1.push_back(e.GetId());
	});
	REQUIRE(m1 == decltype(m1){ e2.GetId() });

	Query<G> q2(w, { index6 != true });
	q2.PreMatch();
	std::vector<EntityId> m2;
	q2.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		REQUIRE(!e.Get<G>().isX);
		m2.push_back(e.GetId());
	});
	REQUIRE(m2 == decltype(m2){ e1.GetId() });
}
