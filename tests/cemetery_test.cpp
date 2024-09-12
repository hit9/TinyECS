#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "TinyECS.h"

using namespace TinyECS;
using namespace TinyECS_Tests;

TEST_CASE("cemetery/1", "[simple]")
{
	__internal::Cemetery cemetery;
	REQUIRE(!cemetery.Contains(3777));
	REQUIRE(cemetery.Size() == 0);
	cemetery.Add(3777);
	cemetery.Add(273);
	REQUIRE(cemetery.Contains(3777));
	REQUIRE(cemetery.Contains(273));
	REQUIRE(cemetery.Size() == 2);
	REQUIRE(cemetery.Pop() == 3777); // FIFO
	REQUIRE(!cemetery.Contains(3777));
	REQUIRE(cemetery.Pop() == 273);
	REQUIRE(!cemetery.Contains(273));
	REQUIRE(cemetery.Size() == 0);
}

TEST_CASE("cemetery/2", "[allocates new block]")
{
	__internal::Cemetery cemetery;
	EntityShortId		 e = 0;
	for (int i = 0; i < __internal::Cemetery::NumRowsPerBlock; i++)
		cemetery.Add(e++);
	REQUIRE(cemetery.Size() == __internal::Cemetery::NumRowsPerBlock);
	for (int i = 0; i < __internal::Cemetery::NumRowsPerBlock; i++)
		cemetery.Add(e++);
	REQUIRE(cemetery.Size() == 2 * __internal::Cemetery::NumRowsPerBlock);
	EntityShortId e1 = 0x7ffff;
	cemetery.Add(e1);
	REQUIRE(cemetery.Contains(e1));
	e = 0;
	for (int i = cemetery.Size() - 1; i; --i, e++)
	{
		REQUIRE(cemetery.Contains(e));
		REQUIRE(cemetery.Pop() == e);
		REQUIRE(!cemetery.Contains(e));
	}
	REQUIRE(cemetery.Pop() == e1);
}

TEST_CASE("cemetery/3", "[reserve]")
{
	__internal::Cemetery cemetery;
	REQUIRE(cemetery.NumBlocks() == 0);
	cemetery.Reserve(2); // number of blocks
	REQUIRE(cemetery.NumBlocks() == 2);
	REQUIRE(cemetery.Size() == 0);
	cemetery.Add(997);
	cemetery.Add(1828);
	cemetery.Add(23);
	REQUIRE(cemetery.NumBlocks() == 2);
	REQUIRE(cemetery.Size() == 3);
	cemetery.Add(2049);
	REQUIRE(cemetery.NumBlocks() == 3);
	REQUIRE(cemetery.Size() == 4);
	// Test functions should keep working.
	REQUIRE(cemetery.Contains(1828));
	REQUIRE(cemetery.Contains(997));
	REQUIRE(cemetery.Contains(23));
	REQUIRE(cemetery.Contains(1828));

	REQUIRE(cemetery.Pop() == 997);
	REQUIRE(cemetery.Pop() == 1828);
	REQUIRE(cemetery.Pop() == 23);
	REQUIRE(cemetery.Pop() == 2049);
	REQUIRE(cemetery.NumBlocks() == 3); // won't shrink
	REQUIRE(cemetery.Size() == 0);
}
