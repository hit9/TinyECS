#include <catch2/catch_test_macros.hpp>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("component id", "[unit component_test.cc]")
{
	auto aid = Internal::IComponent<A>::GetId();
	auto bid = Internal::IComponent<B>::GetId();
	auto cid = Internal::IComponent<C>::GetId();
	REQUIRE(aid == 0);
	REQUIRE(bid == 1);
	REQUIRE(cid == 2);
	REQUIRE(aid == Internal::IComponent<A>::GetId());
}

TEST_CASE("component sequence", "[unit component_test.cc]")
{
	const auto& s1 = Internal::ComponentSequence<A, B>::GetSignature();
	const auto& s2 = Internal::ComponentSequence<B, C>::GetSignature();
	const auto& s3 = Internal::ComponentSequence<A, B>::GetSignature();
	const auto& s4 = Internal::ComponentSequence<A, B, C>::GetSignature();
	const auto& s5 = Internal::ComponentSequence<C, B, A>::GetSignature();
	REQUIRE(&s1 == &s3);
	REQUIRE(s2 != s1);
	// content equals, but pointer differs
	REQUIRE(s4 == s5);
	REQUIRE(&s4 != &s5);
	// What's more, we know exactly whare they are.
	REQUIRE(s4[0]);
	REQUIRE(s4[1]);
	REQUIRE(s4[2]);
	REQUIRE(!s4[3]);
}
