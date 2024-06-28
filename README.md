tinyecs
=======

A simple archetype based ECS library in C++.

DO NOT USE IT IN PRODUCTION.

Requirements
------------

At least `c++20`.

Quick Example
-------------

Checkout [example/main.cc](example/main.cc) please.

Concepts and Internals in brief
-------------------------------

A world is composed of multiple archetypes:

```cpp
class World {
  std::vector<std::unique_ptr<Archetype>> archetypes;
};
```

An archetypes stores entities in a table of blocks,
of which each block is a 2D table of entities X components.

```cpp
class Archetype {
  //        +------------------- Cell x numCols ------------------+
  // Row(0) | EntityReference(0) | Component A | Component B ...  |
  // Row(1) | EntityReference(1) | Component A | Component B ...  |
  //        +-----------------------------------------------------+
  std::vector<std::unique_ptr<unsigned char[]>> blocks;

  Cemetery cemetry;
  // ordered short ids of alive entities for better iteration.
  std::set<EntityShortId> alives;
  // for delayed entity creations and kills.
  std::unordered_map<EntityShortId, Accessor> toBorn, toKill;
};
```

And at the head of each row, we store an `EntityReference`,
which is a reference-like structure to access entity's data.

```cpp
tinyecs::Query<Position, Velocity> q(world);
q.ForEach([](tinyecs::EntityReference &e) {
  auto &position = e.Get<Position>();
  auto &velocity = e.Get<Velocity>();
  position.x += velocity.x;
  position.y += velocity.y;
});
```

Dead entities are collected into a cemetery for recycle purpose:

```cpp
class Cemetery {
  // FIFO reuse.
  std::deque<EntityShortId> q;
  // Fast existence checking and less memory occupy.
  std::vector<std::unique_ptr<std::bitset<NumRowsPerBlock>>> blocks;
}
```

Entity life cycle diagram. We can kill or create entities delayed (e.g. at the begin or end of a frame).

```cpp
//                              +----------------------------------------+
//                              |                   Kill                 |
//                              +                                        |
//                    Apply     |      DelayedKill             Apply     v
//          {toBorn} ------> {alives} ------------> {toKill} -------> {cemetery}
//             ^               ^  ^                                      |
//  DelayedNew |           New |  |           Recycle id                 |
//                       ------+  +--------------------------------------+
```

Documents
---------

- [Components](#Components)

#### Components

Components are just C++ structs:

```cpp
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
```

####


License
-------

BSD
