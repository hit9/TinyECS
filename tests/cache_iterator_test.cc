#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("cache_iterator")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<D>();
	auto& a2 = w.NewArchetype<D, F>();

	auto e1 = a1.NewEntity();
	e1.Get<D>().x = 3;

	auto e2 = a2.NewEntity();
	e2.Get<D>().x = 9;
	e2.Get<F>().status = Status::S3;

	auto e3 = a2.NewEntity();
	e3.Get<D>().x = 18;
	e3.Get<F>().status = Status::S2;

	Query<D> q(w, { index1 >= 3, index1 <= 18 });
	auto	 c = q.PreMatch().Cache();
	for (auto it = c.Begin(); it != c.End(); ++it)
	{
		auto& e = it->second;
		REQUIRE(e.Get<D>().x >= 3);
		REQUIRE(e.Get<D>().x <= 18);
	}
}
