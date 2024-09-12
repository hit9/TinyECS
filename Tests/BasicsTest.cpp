#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "TinyECS.h"

using namespace TinyECS;

TEST_CASE("pack/unpack", "[basic]")
{
	ArchetypeId	  aid = 123;
	EntityShortId eshorId = 34567;
	EntityId	  eid = 64521991;
	REQUIRE(Internal::Pack(aid, eshorId) == eid);
	REQUIRE(Internal::UnpackX(eid) == aid);
	REQUIRE(Internal::UnpackY(eid) == eshorId);

	REQUIRE(Internal::Pack(0b1111, 0b11) == 0b11110000000000000000011);
	REQUIRE(Internal::UnpackX(0b11110000000000000000011) == 0b1111);
	REQUIRE(Internal::UnpackY(0b11110000000000000000011) == 0b11);

	REQUIRE(Internal::Pack(8191, 524287) == 0xffffffff);
	REQUIRE(Internal::UnpackX(0xffffffff) == 8191);
	REQUIRE(Internal::UnpackY(0xffffffff) == 524287);
}

TEST_CASE("pack/unpack",
	"[sort entities id, the id of entities inside same archetype should be consecutive in order]")
{
	std::vector<ArchetypeId>   aids{ 0, 1, 123, 456, 7899, 8191 };
	std::vector<EntityShortId> shortIds{ 0, 1, 2, 33, 777, 34567, 456781, 0xffffffff };
	std::vector<EntityId>	   eids;
	for (auto aid : aids)
	{
		for (auto shortId : shortIds)
			eids.push_back(Internal::Pack(aid, shortId));
	}
	std::sort(eids.begin(), eids.end());
	int k = 0;
	for (int i = 0; i < aids.size(); i++)
	{
		for (int j = 0; j < shortIds.size(); j++)
		{
			REQUIRE(Internal::Pack(aids[i], shortIds[j]) == eids[k++]);
		}
	}
}
