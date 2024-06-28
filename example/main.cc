// c++ example.cc ../tinyecs.cc -std=c++20

#include "tinyecs.h"
#include <iostream>
#include <string>

tinyecs::UnorderedFieldIndex<std::string> tagIndex;

/////////////////
/// Components
////////////////

struct Position {
  int x, y;
  Position(int x = 0, int y = 0) : x(x), y(y) {}
};

struct Velocity {
  double x, y;
  Velocity(double x = 0, double y = 0) : x(x), y(y) {}
};

struct Sprite {
  std::string asset;
  int width, height, zIndex = 1;
};

struct Health {
  int percentage = 100;
};

struct Tag {
  tinyecs::FieldProxy<std::string, decltype(tagIndex)> name;
  std::string value;
  Tag() { name.BindIndex(tagIndex); }
};

int main(void) {
  tinyecs::World w;

  tagIndex.Bind(w);

  // Create archetypes
  auto &soldier = w.NewArchetype<Position, Velocity, Sprite, Health, Tag>();
  auto &tree = w.NewArchetype<Position, Sprite>();
  auto &pedestrian = w.NewArchetype<Position, Velocity, Sprite, Tag>();

  // Create entities.
  soldier.NewEntity([](tinyecs::EntityReference &e) {
    e.Construct<Position>(10, 10);
    e.Construct<Velocity>(5.0, 10.0);
    e.Construct<Tag>();
    auto &tag = e.Get<Tag>();
    tag.name = "x";
    tag.value = "1";
  });
  soldier.NewEntity();

  tree.NewEntity();

  pedestrian.NewEntity([](tinyecs::EntityReference &e) {
    e.Construct<Tag>();
    auto &tag = e.Get<Tag>();
    tag.name = "x";
    tag.value = "2";
  });

  // Query movable entities.
  tinyecs::Query<Position, Velocity> q1(w);
  q1.PreMatch();

  q1.ForEach([](tinyecs::EntityReference &e) {
    auto &position = e.Get<Position>();
    auto &velocity = e.Get<Velocity>();
    position.x += velocity.x;
    position.y += velocity.y;
  });

  // Query by tag index.
  tinyecs::Query<Tag> q2(w);
  q2.PreMatch();
  q2.Where(tagIndex == "x").ForEach([](tinyecs::EntityReference &e) {
    std::cout << e.UncheckedGet<Tag>().value << std::endl;
  });


  // Make a cacher from query q1.
  auto cacher = q1.Cache();
  // Changes are applied to the cacher automatically and incrementally.
  // e.g. Let us creates one more soldier.
  auto &s = soldier.NewEntity();
  s.Get<Position>().x = 10010;
  q1.ForEach([](tinyecs::EntityReference &e) {
    const auto &pos = e.UncheckedGet<Position>();
    std::cout << pos.x << "," << pos.y << std::endl;
  });

  return 0;
}
