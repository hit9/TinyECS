#include <catch2/catch_test_macros.hpp>
#include <unordered_set>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("query_reverse/1", "[query foreach reverse without filters]")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<A>();
	auto& a2 = w.NewArchetype<A, B>();
	auto& a3 = w.NewArchetype<A, B, C>();

	auto e1 = a1.NewEntity([](EntityReference& e) { e.Construct<A>(0, 1); });
	auto e2 = a1.NewEntity([](EntityReference& e) { e.Construct<A>(3, 1); });
	auto e3 = a2.NewEntity([](EntityReference& e) {
		e.Construct<A>(4, 1);
		e.Construct<B>("abc");
	});
	auto e4 = a3.NewEntity([](EntityReference& e) {
		e.Construct<A>(5, 1);
		e.Construct<B>("abc");
		e.Construct<C>(13);
	});

	Query<A>					 q1(w);
	std::vector<EntityReference> vec1;
	q1.PreMatch().ForEach([&vec1](EntityReference& e) { vec1.push_back(e); }, true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3, e2, e1 });

	vec1.clear();
	q1.Collect(vec1, true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3, e2, e1 });

	int cnt = 2;
	vec1.clear();
	q1.PreMatch().ForEachUntil(
		[&vec1, &cnt](EntityReference& e) {
			if (cnt--)
			{
				vec1.push_back(e);
				return false;
			}
			else
				return true;
		},
		true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3 });

	vec1.clear();
	q1.CollectUntil(
		vec1,
		[&vec1](EntityReference& e) {
			if (vec1.size() == 3)
				return true;
			return false;
		},
		true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3, e2 });
}

TEST_CASE("query_reverse/2", "[query foreach reverse with filters]")
{
	World w;
	SETUP_INDEX;

	auto& a1 = w.NewArchetype<D>();
	auto& a2 = w.NewArchetype<D, E>();
	auto& a3 = w.NewArchetype<D, F, E>();

	auto e1 = a1.NewEntity([](EntityReference& e) { e.Construct<D>(2); });
	auto e2 = a2.NewEntity([](EntityReference& e) {
		e.Construct<D>(3);
		e.Construct<E>(3.18, 4, "xyz");
	});
	auto e3 = a2.NewEntity([](EntityReference& e) {
		e.Construct<D>(4);
		e.Construct<E>(3.18, 6, "zzz");
	});

	auto e4 = a3.NewEntity([](EntityReference& e) {
		e.Construct<D>(5);
		e.Construct<E>(3.18, 9, "zzz");
		e.Construct<F>(Status::S3);
	});

	Query<D> q1(w);
	q1.PreMatch();
	q1.Where(index1 >= 3);
	std::vector<EntityReference> vec1;
	q1.ForEach([&vec1](EntityReference& e) { vec1.push_back(e); }, true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3, e2 });

	vec1.clear();
	q1.ForEachUntil(
		[&vec1](EntityReference& e) {
			if (vec1.size() >= 2)
				return true;
			vec1.push_back(e);
			return false;
		},
		true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3 });

	vec1.clear();
	q1.Collect(vec1, true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3, e2 });

	vec1.clear();
	q1.CollectUntil(vec1, [&vec1](EntityReference& e) { return vec1.size() >= 2; }, true);
	REQUIRE(vec1 == std::vector<EntityReference>{ e4, e3 });
}
