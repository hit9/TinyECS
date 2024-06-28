tinyecs
=======

A simple archetype based ECS library in C++.

Requirements: at least C++20.

⚠️  DO NOT USE IT IN PRODUCTION.

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

tinyecs::World w;
```

An archetype is a composite type of multiple component types:

```cpp
// components
struct Position {};
struct Velocity {};
struct Sprite {};

// monster is an archetype with velocity and position attributes.
auto &monster = w.NewArchetype<Velocity, Position>();
```

We create a new entity from its archetype:

```cpp
// Creates an entity and constructs each component by default constructors.
monster.NewEntity();
// Creates an entity and constructs via a custom initializer function.
monster.NewEntity([](tinyecs::EntityReference &e) {
  e.Construct<Position>(10, 10); // Call a constructor of component Position.
  e.Construct<Velocity>(5.0, 10.0);
});
```

An archetype stores its entities in a table of blocks,
of which each block is a 2D table of `entities X components`:

```cpp
class Archetype {
  // For each block's structure:
  //
  //        +------------------- Cell x numCols ------------------+
  // Row(0) | EntityReference(0) | Component A | Component B ...  |
  // Row(1) | EntityReference(1) | Component A | Component B ...  |
  //        +-----------------------------------------------------+
  //
  // Store pointers instead of blocks directly to avoid data copy during
  // vector's capacity growing.
  std::vector<std::unique_ptr<unsigned char[]>> blocks;

  // dead entities for further reuse.
  Cemetery cemetry;
  // ordered short ids of alive entities for better iteration.
  std::set<EntityShortId> alives;
  // for delayed entity creations and kills.
  std::unordered_map<EntityShortId, Accessor> toBorn, toKill;
};
```

A new block is allocated on demand. By default, a block consists of 1024 rows (entities).
If you worry about the dynamic allocation's performance, there's also a `Reserve(numEntities)` method available,
it pre-allocates enough blocks for given number of entities.

At the head of each row, we store an `EntityReference`,
which is a reference-like structure helps to access entity's data.
Instead of obtaining out the whole entity for further operations,
the entity reference is more lightweight.
It doesn't store the data content, but only store the id, data address of the entity.

```cpp
class EntityReference {
  Archetype* a; // pointer to its archetype.
  unsigned char *data; // data address of the data row in the block
  EntityId id;
};
```

For example we can use it in a query,
where the `e` is a reference to the EntityReference at the data row's head.
What's more, there's no `EntityReference` construction and copying here,
we just use the stored entity reference (at the row's head) directly.

```cpp
// e is a reference to the EntityReference stored at the data row's head.
q.ForEach([](tinyecs::EntityReference &e) {
  auto &position = e.Get<Position>();
  auto &velocity = e.Get<Velocity>();
  position.x += velocity.x;
  position.y += velocity.y;
});
```

Every entity owns a unique 32bits integral id, which is composed of two parts:
13bits for its archetype's id, and the other 19 bits for its short id inside the archetype.

```cpp
//                    <x>                         <y>
// 32bits = [ archetype id (13bits)  ][  short entity id (19bits) ]
```

In this design of format, encoding and decoding is fast and simple.

```cpp
// Encode an archetype id (x) and short entity id (y) to an entity id.
eid = (x & 0x1fff) << 19) | (y & 0x7ffff);
// Decode from an archetype id eid to x and y
x = (eid >> 19) & 0x1fff;
y = eid & 0x7ffff;
```

The short entity id indicates which row is to store the entity's data.

```cpp
// This entity should be stored at the i'th block, at j'th row.
auto [i, j] = std::ldiv(shortId, blockSize);
```

Dead entities are collected into a cemetery for recycle purpose.
The entity first dead will be first reused, this is guaranteed by a `deque` structure.
And a table of bitset blocks makes it faster to check the existence of a short entity id,
which occupies fewer memory than unordered sets at the same time.

```cpp
class Cemetery {
  // FIFO reuse.
  std::deque<EntityShortId> q;
  // Fast existence checking and less memory occupy.
  std::vector<std::unique_ptr<std::bitset<NumRowsPerBlock>>> blocks;
}
```

Thus it's `O(1)` to check whether an entity is alive.

```cpp
w.IsAlive(eid);
e.IsAlive();
```

There are 4 possible stages for an entity's lifetime.
We can kill or create entities that take effects later, e.g.
at the end of current frame or the begin of next frame.

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

Here's an example of `DelayedNewEntity()`

```cpp
DelayedNewEntity([](EntityReference &e) {
  // Specific data fields's values when the construction takes effects later.
  e.Construct<SomeComponentX>(233);
});
// Later, e.g. the end of a frame, apply all the delayed stuffs in the world:
w.ApplyDelayedNewEntities();
// w.ApplyDelayedKills() for delayed kills
```

There's a `matcher` in the `world`, it answers matched archetypes by giving a sequence of component classes.
For an example, we may query all entities that contain some components,
and the first step we should do is to query the matched archetypes.
The matching may be slow on large components set, and we **force to cache** the result at the start-up stage via a `PreMatch()` call.
At the runtime stage, the matching will be always skipped.

```cpp
tinyecs::Query<A, B, C> q(w);
// Match archetypes and cache, this call should be placed at ths startup stage.
q.PreMatch();

// At runtime stage
q.Where(SomeIndex == "x").ForEach([](tinyecs::EntityReference &e) { ... });
```

The order of a query's `ForEach()` iteration is according to the entity ids from small to large,
and entities in the same archetype will be accessed next to each other naturally, in the order of the arrangement in the block table,
thanks to the ID encoding design.

```cpp
// archetype a1 [ e1 ] [ e2 ] [ e3 ]
// archetype a2 [ e1 ] [ e2 ] [ e3 ]
// The iteration is from small archetype id to larger,
// and small short entity id to larger inside each archetype.
// a1e1 -> a1e2 -> a1e3 -> a2e1 -> a2e2 -> a3e3
q.ForEach([](tinyecs::EntityReference &e) { ... });
```

We can filter by indexes in a `Query` object, via the `Where()` method.
This feature allows us to index entities by field value quickly.

An `Index` stores the mappings from field value to entity id.
There're two kinds of index supported: `OrderedFieldIndex` and `UnorderedFieldIndex`.

For a code example, we can use the index feature to implement the `tag` ability:

```cpp
tinyecs::UnorderedFieldIndex<std::string> tagIndex; // std::unordered_multimap based

struct Tag {
  tinyecs::FieldProxy<std::string, decltype(tagIndex)> name;
  std::string value;
  // Bind to the entity that associate with this component, on construction.
  Tag() { name.BindIndex(tagIndex); }
};
```

It's important to note that the `BindIndex()` should be called on an entity's construction.
Either bind it in the default constructor or in a custom initializer (example below).

```cpp
monster.NewEntity([&tagIndex](tinyecs::EntityReference& e) { // Custom initializer.
  e.Construct<Tag>();
  e.Get<Tag>().name.BindIndex(tagIndex);
});
```

Where the `FieldProxy` helps to proxy the reads, writes and operations to the indexed field value.
For an instance, the expression `tagIndex == "x"` produces a `Filter` object to be used in a query.
There're many more of operators supported (like `<,<=,>=,>,in,between` etc.).
And currently only `string`, `integral` and `enum` are supported to be "index-able" field types.

Never forget to bind the index to the world at the beginning of a program's startup:

```cpp
// Bind to a world on start-up stage.
tagIndex.Bind(w);
```

We can then query the entities by an index, it should be much faster than checking over each entity:

```cpp
// Query by tag index.
tinyecs::Query<Tag> q(w);
q.PreMatch();

q.Where(tagIndex == "player") // O(1) query
 .ForEach([](tinyecs::EntityReference &e) {
   std::cout << e.UncheckedGet<Tag>().value << std::endl;
 });
```

In reality, we may always perform the same query, e.g. in a certain system.
We can cache a query, in this way, changes are applied to the query results incrementally and automatically.

```cpp
// Make a cacher from the query.
auto cacher = q.Cache();
// Changes are applied to the cacher automatically and incrementally.
// e.g. Let us creates one more soldier, it will be added to the
// cache container automatically.
auto &s = soldier.NewEntity();
s.Get<Position>().x = 10010;

q.ForEach([](tinyecs::EntityReference &e) {
  const auto &pos = e.UncheckedGet<Position>();
  std::cout << pos.x << "," << pos.y << std::endl;
});
```

At the end, there're also `tinyecs::QueryAny<...>` and `tinyecs::QueryNone<...>` available.

The above is the all about tinyecs, It should probably be called EC instead of ECS, since there's no `System`'s role here,
more like a data query thing.


License
-------

BSD
