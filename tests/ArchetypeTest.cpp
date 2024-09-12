
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <stdexcept>

#include "Shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("archetype/1", "[simple]")
{
	World w;
	auto& a = w.NewArchetype<A, B>();
	REQUIRE(a.GetId() == 0);
	REQUIRE(a.BlockSize() == (2 + 1) * MaxNumEntitiesPerBlock * std::max({ sizeof(A), sizeof(B), sizeof(EntityReference) }));

	auto& ref = a.NewEntity();
	auto  eid = ref.GetId();

	REQUIRE(__internal::UnpackX(eid) == a.GetId());
	REQUIRE(__internal::UnpackY(eid) == 0);

	REQUIRE(ref.IsAlive());
	REQUIRE(w.IsAlive(eid));
	REQUIRE(!w.IsAlive(12301));

	int		 cnt = 0;
	Accessor cb = [&](EntityReference& e) {
		REQUIRE(e.IsAlive());
		REQUIRE(e.GetId() == eid);
		REQUIRE(e.GetArchetypeId() == a.GetId());
		REQUIRE(e.Get<A>().x == 0);
		REQUIRE(e.Get<A>().y == 1);
		REQUIRE(e.Get<B>().s == "abc");
		cnt++;
	};

	a.ForEach(cb);
	REQUIRE(cnt == 1);
	REQUIRE(cnt == a.NumEntities());

	w.Get(eid).Get<A>().x = 3;
	REQUIRE(w.Get(eid).Get<A>().x == 3); // reget
	Accessor cb1 = [&](EntityReference& e) {
		REQUIRE(e.Get<A>().x == 3);
		return false;
	};
	a.ForEach(cb1);

	REQUIRE(w.Get(eid).IsAlive());
	w.Kill(eid);
	REQUIRE(!w.IsAlive(eid));
	REQUIRE(!w.Get(eid).IsAlive());
	cnt = 0;
	a.ForEach(cb);
	REQUIRE(cnt == 0);
	REQUIRE(cnt == a.NumEntities());

	REQUIRE(a.NewEntity().GetId() == eid); // recycle
	w.Get(eid).Get<A>().x = 0;			   // back to default value
}

TEST_CASE("archetype/2", "[allocate new block]")
{
	World w;
	auto& a = w.NewArchetype<A, B>();
	for (int i = 0; i < MaxNumEntitiesPerBlock; i++)
		a.NewEntity();
	REQUIRE(a.NumBlocks() == 1);
	REQUIRE(MaxNumEntitiesPerBlock == a.NumEntities());

	for (int i = 0; i < MaxNumEntitiesPerBlock; i++)
		a.NewEntity();
	REQUIRE(a.NumBlocks() == 2);
	REQUIRE(MaxNumEntitiesPerBlock * 2 == a.NumEntities());

	a.NewEntity();
	REQUIRE(a.NumBlocks() == 3);
	REQUIRE(MaxNumEntitiesPerBlock * 2 + 1 == a.NumEntities());

	// Creates a new Entity.
	auto e = a.NewEntity();
	e.Get<A>().x = 333;

	// Gets the data again.
	REQUIRE(e.IsAlive());
	REQUIRE(e.Get<A>().x == 333);

	// random get an entity set and get.
	std::mt19937								 rd(std::random_device{}());
	std::uniform_int_distribution<EntityShortId> dist(0, e.GetId());
	for (int k = 0; k < 100; k++)
	{
		auto eid = dist(rd);
		auto x = dist(rd);
		w.Get(eid).Get<A>().x = x;
		REQUIRE(w.Get(eid).Get<A>().x == x);
	}
}

TEST_CASE("archetype/3", "[constructors and desctructors]")
{
	World w;
	auto& a = w.NewArchetype<A, K>();
	auto  e = a.NewEntity();
	REQUIRE(e.Get<K>().a == 1); // constructor called
	REQUIRE(e.Get<K>().b == 3); // constructor called
	kDescructorCalled = false;
	e.Kill();
	REQUIRE(kDescructorCalled); // desctructor called
}

TEST_CASE("archetype/3", "[desctructor should be called on custom initializer]")
{
	World w;
	auto& a = w.NewArchetype<A, K>();
	auto  e = a.NewEntity([&](EntityReference& e) {
		 e.Construct<A>();
		 e.Construct<K>();
	 });
	REQUIRE(e.Get<K>().a == 1); // constructor called
	REQUIRE(e.Get<K>().b == 3); // constructor called
	kDescructorCalled = false;
	e.Kill();
	REQUIRE(kDescructorCalled); // desctructor called
}

TEST_CASE("archetype/4", "[constructors and desctructors index bind]")
{
	World w;
	SETUP_INDEX;
	auto& a = w.NewArchetype<D, F>();
	auto  e = a.NewEntity();
	REQUIRE(e.Get<D>().x == 0);
	REQUIRE(e.Get<F>().status == Status::S1);
	REQUIRE(e.Get<D>().x.IsBind());
	REQUIRE(e.Get<F>().status.IsBind());
}

TEST_CASE("archetype/6", "[test get unknown column]")
{
	World w;
	SETUP_INDEX;
	auto& a = w.NewArchetype<A>();
	auto  e = a.NewEntity();
	REQUIRE_THROWS_AS(e.Get<B>(), std::runtime_error);
}

TEST_CASE("archetype/7", "[construct a component by initializer]")
{
	World w;
	SETUP_INDEX;
	auto& a = w.NewArchetype<A, E>();
	auto  e = a.NewEntity([](EntityReference& e) { e.Construct<E>(314, "xyz"); });
	REQUIRE(e.Get<E>().x == 314);
	REQUIRE(e.Get<E>().z == "xyz");
	REQUIRE(index3.IsBind());
	REQUIRE(index2.IsBind());
}

TEST_CASE("archetype/8", "[reserve]")
{
	World w;
	SETUP_INDEX;
	auto& a = w.NewArchetype<A>();
	REQUIRE(a.NumBlocks() == 0);
	a.Reserve(2048);
	REQUIRE(a.NumBlocks() == 2);
	REQUIRE(a.NumEntities() == 0);
	for (int i = 0; i < 2048; i++)
		a.NewEntity();
	REQUIRE(a.NumBlocks() == 2);
	REQUIRE(a.NumEntities() == 2048);
	a.NewEntity(); // e=2048
	REQUIRE(a.NumBlocks() == 3);
	REQUIRE(a.NumEntities() == 2049);
	// Test functions should keep working.
	REQUIRE(w.Get(__internal::Pack(a.GetId(), 2048)).IsAlive());
	REQUIRE(w.Get(__internal::Pack(a.GetId(), 2047)).IsAlive());
	REQUIRE(w.Get(__internal::Pack(a.GetId(), 0)).IsAlive());
	auto e = w.Get(__internal::Pack(a.GetId(), 1023));
	REQUIRE(e.Get<A>().y == 1);
	e.Get<A>().x = 33;
	REQUIRE(e.Get<A>().x == 33);
	e.Kill();
	REQUIRE(!e.IsAlive());
	REQUIRE(a.NewEntity().GetId() == e.GetId()); // reuse
	REQUIRE(a.NumBlocks() == 3);
}
