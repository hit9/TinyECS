#include <catch2/catch_test_macros.hpp>
#include <string_view>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("fieldproxy-string/1", "[test string operators]")
{
	World w;
	SETUP_INDEX;

	auto& a = w.NewArchetype<E>();
	auto  e = a.NewEntity();
	e.Get<E>().z += "abc";
	REQUIRE(e.Get<E>().z == "abcabc");
	REQUIRE(e.Get<E>().z.GetValue().size() == 6);
	e.Get<E>().z = std::string_view("zh");
	REQUIRE(e.Get<E>().z == "zh");
	e.Get<E>().z += std::string_view("zh");
	REQUIRE(e.Get<E>().z == "zhzh");
	e.Get<E>().z += std::string("zh");
	REQUIRE(e.Get<E>().z == "zhzhzh");
}
