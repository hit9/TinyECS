#include <catch2/catch_test_macros.hpp>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("cache_callback", "[callback registeration and removal]")
{
	World w;
	SETUP_INDEX;

	auto& a = w.NewArchetype<D>();
	auto  e = a.NewEntity();
	e.Get<D>().x = 1;

	Query<D> q(w);
	q.PreMatch().Where(index1 >= 1);

	{ // open a new scope
		auto c = q.Cache();

		REQUIRE(w.NumCallbacks() == 2); // callback should registered
		REQUIRE(index1.NumCallbacks() == 1);
	}

	// callback should be removed
	REQUIRE(w.NumCallbacks() == 0);
	REQUIRE(index1.NumCallbacks() == 0);
}
