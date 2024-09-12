#include <catch2/catch_test_macros.hpp>

#include "Shares.h"
#include "TinyECS.h"
#include <vector>

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("cache_sort", "[custom compare]")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<D>();
	auto& a2 = w.NewArchetype<D, F>();
	auto& a3 = w.NewArchetype<D, E>();

	auto e1 = a1.NewEntity();
	e1.Get<D>().x = 8;
	auto e2 = a1.NewEntity();
	e2.Get<D>().x = 9;
	auto e3 = a2.NewEntity();
	e3.Get<D>().x = 3;
	auto e4 = a3.NewEntity();
	e4.Get<D>().x = 1;
	auto e5 = a3.NewEntity();
	e5.Get<D>().x = 3;

	Query<D> q(w);
	auto	 cmp = [&w](const EntityId a, const EntityId b) {
		auto xa = w.UncheckedGet(a).UncheckedGet<D>().x;
		auto xb = w.UncheckedGet(b).UncheckedGet<D>().x;
		return xa < xb || (xa == xb && a < b);
	};
	auto				  c = q.PreMatch().Cache<decltype(cmp)>(cmp);
	std::vector<EntityId> z;
	c.ForEach([&z](EntityReference& ref) { z.push_back(ref.GetId()); });
	REQUIRE(z == decltype(z){ e4.GetId(), e3.GetId(), e5.GetId(), e1.GetId(), e2.GetId() });
}
