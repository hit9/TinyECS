#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("cache_reverse/1", "[cache reverse without filters]")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<A>();
	auto& a2 = w.NewArchetype<B>();
	auto& a3 = w.NewArchetype<A, B>();

	auto e1 = a1.NewEntity();
	auto e2 = a1.NewEntity();
	auto e3 = a2.NewEntity();
	auto e4 = a2.NewEntity();
	auto e5 = a3.NewEntity();

	Query<B>					 q(w);
	auto						 cacher = q.PreMatch().Cache();
	std::vector<EntityReference> vec;
	cacher.ForEach([&vec](EntityReference& e) { vec.push_back(e); }, true);
	REQUIRE(vec == std::vector<EntityReference>{ e5, e4, e3 });

	auto e6 = a3.NewEntity();
	vec.clear();
	cacher.ForEach([&vec](EntityReference& e) { vec.push_back(e); }, true);
	REQUIRE(vec == std::vector<EntityReference>{ e6, e5, e4, e3 });

	e4.Kill();
	vec.clear();
	cacher.ForEach([&vec](EntityReference& e) { vec.push_back(e); }, true);
	REQUIRE(vec == std::vector<EntityReference>{ e6, e5, e3 });

	vec.clear();
	cacher.Collect(vec, true);
	REQUIRE(vec == std::vector<EntityReference>{ e6, e5, e3 });

	vec.clear();
	cacher.CollectUntil(
		vec,
		[&vec](EntityReference& e) {
			if (vec.size() == 2)
				return true;
			return false;
		},
		true);
	REQUIRE(vec == std::vector<EntityReference>{ e6, e5 });
}

TEST_CASE("cache_reverse/2", "[cache reverse with filters]")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<D, E>();
	auto& a2 = w.NewArchetype<E>();
	auto& a3 = w.NewArchetype<E, F>();

	auto e1 = a1.NewEntity([](EntityReference& e) { e.Construct<E>(3.10, 1, "xyz"); });
	auto e2 = a2.NewEntity([](EntityReference& e) { e.Construct<E>(3.10, 2, "xyz"); });
	auto e3 = a2.NewEntity([](EntityReference& e) { e.Construct<E>(3.10, 3, "xyz"); });
	auto e4 = a3.NewEntity([](EntityReference& e) { e.Construct<E>(3.2, 4, "abc"); });

	Query<E> q(w);
	q.PreMatch();
	q.Where(index3 >= 2);

	std::vector<EntityReference> vec;
	q.ForEach([&](EntityReference& e) { vec.push_back(e); }, true);
	REQUIRE(vec == std::vector<EntityReference>{ e4, e3, e2 });

	vec.clear();
	q.ForEachUntil(
		[&vec](EntityReference& e) {
			if (vec.size() >= 2)
				return true;
			vec.push_back(e);
			return false;
		},
		true);
	REQUIRE(vec == std::vector<EntityReference>{ e4, e3 });

	vec.clear();
	q.Collect(vec, true);
	REQUIRE(vec == std::vector<EntityReference>{ e4, e3, e2 });

	vec.clear();
	q.CollectUntil(vec, [&vec](EntityReference& e) { return vec.size() >= 2; }, true);
	REQUIRE(vec == std::vector<EntityReference>{ e4, e3 });

	auto e5 = a3.NewEntity([](EntityReference& e) { e.Construct<E>(3.2, 5, "abc"); });
	vec.clear();
	q.Collect(vec, true);
	REQUIRE(vec == std::vector<EntityReference>{ e5, e4, e3, e2 });

	e3.Kill();
	vec.clear();
	q.Collect(vec, true);
	REQUIRE(vec == std::vector<EntityReference>{ e5, e4, e2 });
}
