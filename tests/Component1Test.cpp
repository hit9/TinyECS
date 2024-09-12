#include <catch2/catch_test_macros.hpp>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("component id", "[unit component1_test.cc]")
{
	auto aid = __internal::IComponent<A>::GetId();
	auto bid = __internal::IComponent<B>::GetId();
	auto cid = __internal::IComponent<C>::GetId();
	REQUIRE(aid == 0);
	REQUIRE(bid == 1);
	REQUIRE(cid == 2);
	REQUIRE(aid == __internal::IComponent<A>::GetId());
}

TEST_CASE("component sequence", "[unit component1_test.cc]")
{
	const auto& s1 = __internal::ComponentSequence<A, B, C>::GetSignature();
	const auto& s2 = __internal::ComponentSequence<C, B, A>::GetSignature();
	const auto& s3 = __internal::ComponentSequence<D, B, A>::GetSignature();
	REQUIRE(s1 == s2);
	REQUIRE(s1 != s3);
	REQUIRE(s1[0]);
	REQUIRE(s1[1]);
	REQUIRE(s1[2]);
	REQUIRE(!s1[3]);
}
