// c++ example.cc ../TinyECS.cc -std=c++20

#include "TinyECS.h"
#include <iostream>
#include <string>

TinyECS::UnorderedFieldIndex<std::string> tagIndex;

/////////////////
/// Components
////////////////

struct Position
{
	int x, y;
	Position(int x = 0, int y = 0)
		: x(x), y(y) {}
};

struct Velocity
{
	double x, y;
	Velocity(double x = 0, double y = 0)
		: x(x), y(y) {}
};

struct Sprite
{
	std::string asset;
	int			width, height, zIndex = 1;
};

struct Health
{
	int percentage = 100;
};

struct Tag
{
	TinyECS::FieldProxy<std::string, decltype(tagIndex)> name;
	std::string											 value;
	Tag() { name.BindIndex(tagIndex); }
};

int main(void)
{
	TinyECS::World w;

	tagIndex.Bind(w);

	// Create archetypes
	auto& soldier = w.NewArchetype<Position, Velocity, Sprite, Health, Tag>();
	auto& tree = w.NewArchetype<Position, Sprite>();
	auto& pedestrian = w.NewArchetype<Position, Velocity, Sprite, Tag>();

	// Create entities.
	soldier.NewEntity([](TinyECS::EntityReference& e) {
		e.Construct<Position>(10, 10);
		e.Construct<Velocity>(5.0, 10.0);
		e.Construct<Tag>();
		auto& tag = e.Get<Tag>();
		tag.name = "x";
		tag.value = "1";
	});
	soldier.NewEntity();

	tree.NewEntity();

	pedestrian.NewEntity([](TinyECS::EntityReference& e) {
		e.Construct<Tag>();
		auto& tag = e.Get<Tag>();
		tag.name = "x";
		tag.value = "2";
	});

	// Query movable entities.
	TinyECS::Query<Position, Velocity> q1(w);
	q1.PreMatch();

	q1.ForEach([](TinyECS::EntityReference& e) {
		auto& position = e.Get<Position>();
		auto& velocity = e.Get<Velocity>();
		position.x += velocity.x;
		position.y += velocity.y;
	});

	// Query by tag index.
	TinyECS::Query<Tag> q2(w);
	q2.PreMatch();
	q2.Where(tagIndex == "x").ForEach([](TinyECS::EntityReference& e) {
		std::cout << e.UncheckedGet<Tag>().value << std::endl;
	});

	// Make a cacher from query q1.
	auto cacher = q1.Cache();
	// Changes are applied to the cacher automatically and incrementally.
	// e.g. Let us creates one more soldier.
	auto& s = soldier.NewEntity();
	s.Get<Position>().x = 10010;
	q1.ForEach([](TinyECS::EntityReference& e) {
		const auto& pos = e.UncheckedGet<Position>();
		std::cout << pos.x << "," << pos.y << std::endl;
	});

	return 0;
}
