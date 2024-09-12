#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("query_filters_operator_between")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<D>();
	auto& a2 = w.NewArchetype<F>();
	auto& a3 = w.NewArchetype<D, F>();

	auto e1 = a1.NewEntity();
	auto e2 = a1.NewEntity();
	auto e3 = a2.NewEntity();
	auto e4 = a3.NewEntity();

	e1.Get<D>().x = 34;
	e2.Get<D>().x = 84;
	e3.Get<F>().status = Status::S3;
	e4.Get<D>().x = 44;
	e4.Get<F>().status = Status::S3;

	// query D.x between 3;
	Query<D> q1(w, { index1.Between({ 44, 84 }) });
	q1.PreMatch();
	std::vector<EntityId> m1;
	q1.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		auto x = e.Get<D>().x;
		REQUIRE(x >= 44);
		REQUIRE(x <= 84);
		m1.push_back(e.GetId());
	});
	REQUIRE(m1 == decltype(m1){ e2.GetId(), e4.GetId() });

	// query D.x between 3 && F.status == S3;
	Query<D, F> q2(w, { index1.Between({ 44, 84 }), index5 == Status::S3 });
	q2.PreMatch();
	std::vector<EntityId> m2;
	q2.ForEach([&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		auto x = e.Get<D>().x;
		REQUIRE(x >= 44);
		REQUIRE(x <= 84);
		REQUIRE(e.Get<F>().status == Status::S3);
		m2.push_back(e.GetId());
	});
	REQUIRE(m2 == decltype(m2){ e4.GetId() });
}
