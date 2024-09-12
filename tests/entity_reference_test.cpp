#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("entity_reference/1", "[entity reference validation]")
{
	// an entity reference should be valid as long as the entity itself is aliv.
	World w;
	auto& a = w.NewArchetype<A>();
	REQUIRE(!w.Get(0).IsAlive());
	auto& e = a.NewEntity();
	auto& e1 = w.Get(e.GetId());
	auto& e2 = w.Get(e.GetId());
	REQUIRE(e == e1);
	REQUIRE(e == e2);
	REQUIRE((&e) == (&e1));
	REQUIRE((&e1) == (&e2));
}
