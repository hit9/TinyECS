// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/tinyecs
// Requirements: at least C++20. Version: 0.1.0

#ifndef __HIT9_TINYECS_H
#define __HIT9_TINYECS_H

#include <algorithm>     // std::max
#include <bitset>        // std::bitset
#include <concepts>      // std::integral
#include <cstddef>       // std::size_t
#include <cstdint>       // uint32_t, uint16_t
#include <deque>         // std::deque
#include <functional>    // std::function
#include <map>           // std::map, std::multimap
#include <memory>        // std::unique_ptr, std::shared_ptr
#include <set>           // std::set
#include <stdexcept>     // std::runtime_error
#include <string>        // std::string
#include <string_view>   // std::string_view
#include <type_traits>   // std::is_convertible_v
#include <unordered_map> // std::unordered_map, std::unordered_multimap
#include <unordered_set> // std::unordered_set
#include <utility>       // std::pair
#include <vector>        // std::vector

namespace tinyecs {

static const size_t MaxNumEntitiesPerBlock = 1024;
static const size_t MaxNumComponents = 128;
static const size_t MaxNumArchetypesPerWorld = 4096; // Enough?

using size_t = std::size_t;
using ComponentId = uint16_t;   // global auto-incremented id for each component class.
using ArchetypeId = uint16_t;   // up to 2^13-1 = 8191 =  0x1fff
using EntityShortId = uint32_t; // up to 2^19-1 = 524287(52.4w) = 0x7ffff
using EntityId = uint32_t;      // Long entity id => packed archetypeId with EntityShortId

using Signature = std::bitset<MaxNumComponents>;

// ~~~~~~~~ Forward declarations. ~~~~~~~~~~
class World;
class EntityReference;
namespace __internal { // DO NOT USE NAMES FROM __internal
class ICacher;
class IQuery;
class IArchetype;
} // namespace __internal

namespace __internal { // DO NOT USE NAMES FROM __internal

using EntityIdSet = std::unordered_set<EntityId>; // set of entity ids
using AIds = std::unordered_set<ArchetypeId>;     // set of archetype ids.
using AIdsPtr = std::shared_ptr<AIds>;            // shared pointer to a set of archetype ids.

// Layout of an entity id:
//                    <x>                         <y>
// 32bits = [ archetype id (13bits)  ][  short entity id (19bits) ]
inline EntityId pack(ArchetypeId x, EntityShortId y) { return ((x & 0x1fff) << 19) | (y & 0x7ffff); }
// Packer and Unpacker between entity id and (archetype id + short entity id)
inline ArchetypeId unpack_x(EntityId eid) { return (eid >> 19) & 0x1fff; }
inline EntityShortId unpack_y(EntityId eid) { return eid & 0x7ffff; }

///////////////////////////
/// Component
///////////////////////////

class IComponentBase {
protected:
  static ComponentId nextId; // initialized in tinyecs.cc
};

template <typename Component> class IComponent : public IComponentBase {
public:
  // Auto-incremented for each component class.
  static const ComponentId GetId() {
    static auto id = nextId++;
    return id;
  }
};

// Component parameters shouldn't contain duplicates, why it works:
// in C++, a class is not allowed to inherit from the same base class twice.
template <typename Component> class NoDuplicateComponents {};

// A class composed of a sequence of component classes.
template <typename... Components> class ComponentSequence : NoDuplicateComponents<Components>... {
  inline static Signature signature;
  inline static bool initialized = false;

public:
  static const size_t N = sizeof...(Components);
  static const size_t MaxComponentSize = std::max({(size_t)1, sizeof(Components)...}); // at least 1

  // Returns the signature of this components sequence.
  // Internal: the signature bitset will be initialized only once for a kind of type.
  static const Signature &GetSignature() {
    if (initialized) return signature;
    std::vector<ComponentId> cids{IComponent<Components>::GetId()...};
    for (auto x : cids)
      signature.set(x);
    initialized = true;
    return signature;
  }
};
} // namespace __internal

//////////////////////////
/// EntityReference
//////////////////////////

// Accessor is a function to access an entity via its entity reference.
using Accessor = std::function<void(EntityReference &)>;
// AccessorUntil is the Accessor that stops the iteration earlier once the callback returns true.
using AccessorUntil = std::function<bool(EntityReference &)>;

namespace __internal {

class IArchetypeEntityApi { // Archetype api for EntityReference.
public:
  virtual ArchetypeId GetId() const = 0;

protected:
  // ~~~~~~ for IArchetype to override ~~~~~~
  virtual bool isAlive(EntityShortId e) const = 0;
  virtual void kill(EntityShortId e, Accessor *cb = nullptr) = 0;
  virtual void delayedKill(EntityShortId e, Accessor *beforeKilledPtr) = 0;
  virtual unsigned char *getComponentRawPtr(unsigned char *data, ComponentId cid) const = 0;
  virtual unsigned char *uncheckedGetComponentRawPtr(unsigned char *data, ComponentId cid) const = 0;
  // ~~~~ templated api method ~~~~~
  template <typename C> inline C *getComponentPtr(unsigned char *data) const {
    return reinterpret_cast<C *>(getComponentRawPtr(data, IComponent<C>::GetId()));
  }
  template <typename C> inline C *uncheckedGetComponentPtr(unsigned char *data) const {
    return reinterpret_cast<C *>(uncheckedGetComponentRawPtr(data, IComponent<C>::GetId()));
  }
  friend EntityReference;
};

} // namespace __internal

// A temporary reference to an entity's data, we should keep it lightweight enough.
class EntityReference {
private:
  __internal::IArchetypeEntityApi *a = nullptr;
  // Internal note: although there's an entity reference stored in the archetype, at the head of this entity's
  // data row. But we still need to store an address of the data rather than casting pointer `this` to get the
  // data pointer, because we allow to copy an entity reference.
  unsigned char *data = nullptr;
  EntityId id = 0;

public:
  EntityReference(__internal::IArchetypeEntityApi *a, unsigned char *data, EntityId id)
      : a(a), data(data), id(id) {}
  EntityReference() = default; // for __internal::NullEntityReference
  ~EntityReference() = default;
  // Returns the id of this entity.
  inline EntityId GetId() const { return id; }
  // Returns the id of this entity's archetype.
  inline ArchetypeId GetArchetypeId() const { return a->GetId(); }
  // Two entity references are equal if their data address is the same.
  bool operator==(const EntityReference &o) const { return data == o.data; }
  // Returns a component of this entity by given component type.
  // Throws a std::runtime_error if the given component is not part of this entity's archetype.
  template <typename Component> inline Component &Get() { return *a->getComponentPtr<Component>(data); }
  // UncheckedGet is similar to Get(), but won't validate column's legality.
  // Use it if the component is guaranteed to be part of this entity's archetype.
  template <typename Component> inline Component &UncheckedGet() {
    return *a->uncheckedGetComponentPtr<Component>(data);
  }
  // Kill this entity right now.
  inline void Kill() { a->kill(__internal::unpack_y(id), nullptr); }
  // Mark this entity to be killed later.
  // Finally we should call world.ApplyDelayedKills() to make it take effect.
  inline void DelayedKill() { a->delayedKill(__internal::unpack_y(id), nullptr); }
  // DelayedKill with a hook function to be called before this entity is applied killed.
  // No matter whether the `beforeKilled` function is provided, the destructor of each component of this
  // entity is going to be called on this entity's death.
  inline void DelayedKill(Accessor &beforeKilled) { a->delayedKill(__internal::unpack_y(id), &beforeKilled); }
  inline void DelayedKill(Accessor &&beforeKilled) { DelayedKill(beforeKilled); }
  // Returns true if this entity is alive.
  // For delay created entity, it's not alive until a world.ApplyDelayedNewEntities() is called.
  // For delay killed entity, it's still alive until a world.ApplyDelayedKills() is called.
  inline bool IsAlive(void) const { return a != nullptr && a->isAlive(__internal::unpack_y(id)); }
  // Constructs a component of this entity.
  // The component class must have a corresponding constructor.
  // And the field index binding must be done in that constructor.
  template <typename Component, typename... Args> void Construct(Args... args) {
    auto ptr = a->getComponentPtr<Component>(data);
    new (ptr) Component(std::forward<Args>(args)...);
  }
};

//////////////////////////
/// IArchetype
//////////////////////////

namespace __internal { // DO NOT USE NAMES FROM __internal

// Internal World interface class.
class IWorld {
protected:
  // ~~~~~~ Hook functions for IArchetype ~~~~
  void setLastCreatedEntityId(EntityId eid);
  inline void clearLastCreatedEntityId() { lastCreatedEntityIdSet = false; }
  // ~~~~~~~~ for class World to override ~~~~~~~
  // trigger callbacks on creating and removing entities.
  virtual void afterEntityCreated(ArchetypeId a, EntityShortId e) = 0;
  virtual void beforeEntityRemoved(ArchetypeId a, EntityShortId e) = 0;
  // for archetype to mark delayed creating and killing entities to the world.
  virtual void addDelayedKillEntity(ArchetypeId a, EntityShortId e) = 0;
  virtual void addDelayedNewEntity(ArchetypeId a, EntityShortId e) = 0;

  friend class IArchetype;
  friend class IFieldIndex; // for lastCreatedEntityId

private:
  EntityId lastCreatedEntityId = 0;
  bool lastCreatedEntityIdSet = false;
};

// Cemetery stores short ids of dead entities.
class Cemetery {
public:
  static const size_t NumRowsPerBlock = MaxNumEntitiesPerBlock;
  inline size_t Size() const { return q.size(); }
  bool Contains(EntityShortId e) const; // O(1)
  void Add(EntityShortId e);            // Worst O(block allocation times), best O(1)
  EntityShortId Pop();                  // O(1)
private:
  // FIFO reuse. use deque instead of list, reason:
  // 1. list: less memory, but invokes a memory allocation on each insertion.
  // 2. deque: linked fixed-size arrays, more memory, at least two pointer dereferences accessing,
  //      but allocates a whole block at once.
  // For liveness checking, we just use the bitset, the queue is only for insertions and removals,
  // so the memory allocation cost matters, the deque wins.
  std::deque<EntityShortId> q;
  // A vector of bitset pointers, for quick existence checking.
  // Use bitsets instead of a single unordered_set for less memory usage.
  // Since the bitset requires a fixed-size template parameter, so we cut it into blocks,
  // storing a vector of pointers of fixed-size bitset units.
  std::vector<std::unique_ptr<std::bitset<NumRowsPerBlock>>> blocks;
  // Redundancy of: blocks.size() * NumRowsPerBlock
  // Perform addition for `bound` on each `Add()`, and the multiplication operation in the method Contains()
  // could be omitted. Contains() calls requires to be fast.
  size_t bound = 0;
};

// Conversion functions between entity reference pointer and raw data pointer.
inline auto toref(unsigned char *data) { return reinterpret_cast<EntityReference *>(data); }
inline auto todata(EntityReference *ref) { return reinterpret_cast<unsigned char *>(ref); }

// Internal Archetype interface class.
class IArchetype : public IArchetypeEntityApi {
private:
  ArchetypeId id;
  EntityShortId ecursor = 0; // e (short entity id) cursor.
  IWorld *world = nullptr;
  const Signature &signature; // ref to static storage in class ComponentSequence.

  // Schematic diagram of entity life cycle::
  //
  //                              +----------------------------------------+
  //                              |                   Kill                 |
  //                              +                                        |
  //                    Apply     |      DelayedKill             Apply     v
  //          {toBorn} ------> {alives} ------------> {toKill} -------> {cemetery}
  //             ^               ^  ^                                      |
  //  DelayedNew |           New |  |           Recycle id                 |
  //                       ------+  +--------------------------------------+

  // use an ordered set to store short ids of alive entities,
  // for faster ForEach(), than iterating one by one in a block.
  std::set<EntityShortId> alives;
  // for id and space recycle & fast liveness checkings:
  // IsAlive: !toBorn && !cemetery; both O(1)
  Cemetery cemetery;
  // [ DelayedNew ]
  //   Mark: stores data directly in blocks and add to map toBorn (not alives).
  //   Apply: move from map toBorn to set alives, wthout copying entity data.
  // [ DelayedKill ]
  //   Mark: add to map toKill.
  //   Apply: move from set alives to cemetery, and remove from toKill.
  // use an unordered_map instead of an unordered_set for toBorn, stores the callbacks.
  std::unordered_map<EntityShortId, Accessor> toBorn, toKill;

  // Use a fixed-size array for faster performance than an unordered_map.
  // Because it's very often to get a component of an entity reference.
  // It's supposed that the total number of components is very limited.
  // cols[componentID] => column number in this archetype.
  // Max memory usage estimate in a world:
  // N(archetypes) x N(components) * sizeof(uint16_t) = 4096 * 128 * 2 = 1MB
  static const size_t numRows = MaxNumEntitiesPerBlock;
  const size_t numCols, cellSize, rowSize, blockSize;
  uint16_t cols[MaxNumComponents];
  // Block layout:
  //
  //        +------------------- Cell x numCols ------------------+
  // Row(0) | EntityReference(0) | Component A | Component B ...  |
  // Row(1) | EntityReference(1) | Component A | Component B ...  |
  //        +-----------------------------------------------------+
  //                Block(0), height=<numRows>, width=<numCols>
  //
  // The vector stores pointers of block buffer, not the block itself,
  // avoiding buffer copying during vector capacity growing.
  std::vector<std::unique_ptr<unsigned char[]>> blocks;

  // Returns a reference to the signature of this archetype.
  inline const Signature &getSignature() const { return signature; };
  // Returns the data address of entity e.
  // It doesn't care the entity's liveness, just return the data address that should be.
  unsigned char *getEntityData(EntityShortId e) const;
  // Get EntityReference by given short entity id.
  // Won't check the entity's liveness.
  inline EntityReference &uncheckedGet(EntityShortId e) { return *toref(getEntityData(e)); }
  // Returns a reference to the target entity by giving a short entity id.
  // If the entity is not alive (dead or not born yet), returns NullEntityReference.
  EntityReference &get(EntityShortId e);

  friend World;   // for get, uncheckedGet, getSignature, kill, isAlive, delayedKill
  friend IQuery;  // for get
  friend ICacher; // for get, uncheckedGet

protected:
  // ~~~~ Implements IArchetypeEntityApi ~~~~~
  unsigned char *getComponentRawPtr(unsigned char *data, ComponentId cid) const override;
  inline unsigned char *uncheckedGetComponentRawPtr(unsigned char *data, ComponentId cid) const override {
    return data + cols[cid] * cellSize;
  }
  // Check if given entity is alive.
  // We use cemetery along with toBorn to test liveness rather than the set alives,
  // because the former is O(1), and the latter is O(logN).
  inline bool isAlive(EntityShortId e) const override {
    return e < ecursor && !cemetery.Contains(e) && !toBorn.contains(e);
  }

  //~~~~~~~~ Kill Entity ~~~~~~~~~~~~~~
  // kill an entity by short id at once. O(logN).
  void kill(EntityShortId e, Accessor *cb) override;
  // Mark an entity to be killed. O(1)
  // If the given entity is already dead, does nothing.
  // Parameter beforeKillPtr is a pointer to a function, which is to be execute before the killing is applied.
  void delayedKill(EntityShortId e, Accessor *beforeKillPtr) override;
  // Kill a delayed to kill entity by short entity id.
  // Does nothing if given entity is not in the toKill map.
  void applyDelayedKill(EntityShortId e);

  //~~~~~~~~ New Entity ~~~~~~~~~~~~~~
  // Allocates a seat for a new entity, including a short id and block row's memory address. Time: O(1)
  //  1. If the cemetery is not empty, reuse at first.
  //  2. Otherwise increases the internal short id cursor.
  //     What's more, allocates a new block on need.
  std::pair<EntityShortId, unsigned char *> allocateForNewEntity();
  // Makes an entity to be alive in the world.
  void doNewEntity(EntityShortId e, EntityReference &ref, Accessor &initializer);
  // Makes an entity to be born alive.
  // Does nothing if given entity is not in the toBorn map.
  void applyDelayedNewEntity(EntityShortId e);

  // ~~~~~~~ for Archetype Impl ~~~~~~~~~~
  // Call default destructors of all components of an entity by providing entity data address.
  virtual void destructComponents(unsigned char *data) = 0;
  // Call default constructors of all components of an entity by providing entity data address.
  virtual void constructComponents(unsigned char *data) = 0;

public:
  IArchetype(ArchetypeId id, IWorld *world, size_t numComponents, size_t cellSize,
             const Signature &signature);
  virtual ~IArchetype() {}
  IArchetype(const IArchetype &o) = delete;
  IArchetype &operator=(const IArchetype &o) = delete;
  inline ArchetypeId GetId() const override { return id; }
  // Returns number of blocks.
  inline size_t NumBlocks() const { return blocks.size(); }
  // Returns the size of a single block.
  inline size_t BlockSize() const { return blockSize; }
  // Returns the number of alive entities in this archetype.
  // Note: cemetery+toBorn should always not larger than ecursor, thus minus on size_t is safe.
  inline size_t NumEntities() const { return ecursor - cemetery.Size() - toBorn.size(); }
  // Creates a new entity at once, try to reuse dead entity's id and memory space at first,
  // otherwise allocating a new seat from underlying block, and allocates a new block on need.
  // Returns a reference to the created entity.
  // Parameter initializer is a function to initialize all component data for the new entity.
  // If the initializer is not provided, the default constructor will be called without any arguments.
  // Time complexity: O(logN) to maintain the set alives.
  EntityReference &NewEntity();
  EntityReference &NewEntity(Accessor &initializer);
  inline EntityReference &NewEntity(Accessor &&initializer) { return NewEntity(initializer); }
  // Creates a new entity later, returns a reference to the entity.
  // Finally we should call world.ApplyDelayedNewEntities() to make it take effect.
  // Returns the pre-allocated entity id.
  // Parameter initializer is a function to initialize all component data for the new entity.
  // Although it's still considered not-alive, but we allocate an entity id and un-initialized space in
  // advance. It's meaningless to get the reference of a to-born entity and set data to it before it's applied
  // to be alive.
  // If the initializer is not provided, the default constructor will be called without any arguments.
  // When the ApplyDelayedNewEntities is called, the entity is just marked alive without any data copy.
  EntityId DelayedNewEntity();
  EntityId DelayedNewEntity(Accessor &initializer);
  inline EntityId DelayedNewEntity(Accessor &&initializer) { return DelayedNewEntity(initializer); }
  // Run given callback function for all alive entities in this archetype.
  // It will iterate entities in order of id from smaller to larger.
  void ForEach(const Accessor &cb);
  // ForEach is allowed to pass in a temporary function.
  // Templates and reference folding are not used here, but overloading is used.
  inline void ForEach(const Accessor &&cb) { ForEach(cb); }
  // ForEachUntil is the ForEach that stops the iteration earlier when the given callback returns true.
  void ForEachUntil(const AccessorUntil &cb);
  inline void ForEachUntil(const AccessorUntil &&cb) { ForEachUntil(cb); }
};

using ArchetypeSignature = std::bitset<MaxNumArchetypesPerWorld>;
static const ArchetypeSignature ArchetypeSignatureNone;

//////////////////////////
/// Matcher
//////////////////////////

enum class MatchRelation { ALL, ANY, NONE };

class Matcher {
public:
  Matcher() = default;
  ~Matcher() = default;

  // Put an archetype id with given signature.
  void PutArchetypeId(const Signature &signature, ArchetypeId aid);
  // Match results for given signature and relation.
  // If given relation is ANY, then empty signature means matching all.
  AIds Match(MatchRelation relation, const Signature &signature) const;
  // Match results and store, returns a shared pointer to the stored result.
  const AIdsPtr MatchAndStore(MatchRelation relation, const Signature &signature);

private:
  using ArchetypeIdBitset = std::bitset<MaxNumArchetypesPerWorld>;
  // stores all archetype ids.
  ArchetypeIdBitset all;
  // b[x] stores all archetype id contains component x.
  ArchetypeIdBitset b[MaxNumComponents];
  // store results for a signature.
  std::vector<AIdsPtr> store;

  ArchetypeIdBitset matchAll(const Signature &signature) const;
  ArchetypeIdBitset matchAny(const Signature &signature) const;
  ArchetypeIdBitset matchNone(const Signature &signature) const;
};
} // namespace __internal

//////////////////////////
/// Archetype Impl
//////////////////////////

template <typename... Components> class Archetype : public __internal::IArchetype {
public:
  Archetype(ArchetypeId id, __internal::IWorld *w)
      : __internal::IArchetype(id, w, CS::N, CellSize, CS::GetSignature()) {}

protected:
  void destructComponents(unsigned char *data) override { (destructComponent<Components>(data), ...); }
  void constructComponents(unsigned char *data) override { (constructComponent<Components>(data), ...); }
  friend World; // for getSignature
private:
  using CS = __internal::ComponentSequence<Components...>;
  static_assert(CS::N, "tinyecs: Archetype requires at least one component type parameter");
  // CellSize is the max size of { components 's sizes, entity reference size }
  static constexpr size_t CellSize = std::max(CS::MaxComponentSize, sizeof(EntityReference));
  template <typename C> void destructComponent(unsigned char *data) { getComponentPtr<C>(data)->~C(); }
  template <typename C> void constructComponent(unsigned char *data) { new (getComponentPtr<C>(data)) C(); }
};

//////////////////////////
/// World
//////////////////////////

namespace __internal {
using CallbackEntityLifecycle = std::function<void(EntityReference &)>; // internal callback
} // namespace __internal

// CallbackAfterEntityCreated is a function to be executed right after an entity is created.
using CallbackAfterEntityCreated = __internal::CallbackEntityLifecycle;
// CallbackBeforeEntityRemoved is a function to be executed right before an entity is removing.
using CallbackBeforeEntityRemoved = __internal::CallbackEntityLifecycle;

class World : public __internal::IWorld {
public:
  World() : matcher(std::make_unique<__internal::Matcher>()) {};
  ~World() = default;
  // Creates a new archetype, returns the reference.
  // The lifetime of returned archetype's reference is as long as the world instance.
  template <typename... Components> [[nodiscard]] Archetype<Components...> &NewArchetype() {
    ArchetypeId id = archetypes.size();
    auto ptr = std::make_unique<Archetype<Components...>>(id, this);
    auto rawpointer = ptr.get();
    matcher->PutArchetypeId(ptr->getSignature(), ptr->GetId());
    archetypes.push_back(std::move(ptr));
    callbackTable[0].resize(archetypes.size());
    callbackTable[1].resize(archetypes.size());
    return *rawpointer;
  }
  // Returns true if given entity is alive.
  [[nodiscard]] bool IsAlive(EntityId eid) const;
  // Kill an entity by id.
  void Kill(EntityId eid);
  // Mark an entity to be killed later.
  // Finally we should call world.ApplyDelayedKills() to make it take effect.
  void DelayedKill(EntityId eid);
  // DelayedKill with an additional callback to be executed before the entity is killed.
  // No matter whether a `beforeKilled` function is provided, the destructor of each component of the entity
  // will be called on the entity's death.
  void DelayedKill(EntityId eid, Accessor &beforeKilled);
  // forward right reference for temporary beforeKilled parameter.
  inline void DelayedKill(EntityId eid, Accessor &&beforeKilled) { DelayedKill(eid, beforeKilled); }
  // Returns the reference to an entity by entity id.
  // Returns the NullEntityReference if given entity does not exist, of which method IsAlive() == false,
  [[nodiscard]] EntityReference &Get(EntityId eid) const;
  // Unchecked version of method Get(), user should ensure the given entity is still alive.
  // Undefined behavior if given entity does not exist.
  [[nodiscard]] EntityReference &UncheckedGet(EntityId eid) const;
  // Register a callback function to be called right after an entity associated with given
  // components is created.
  template <typename... Components> uint32_t AfterEntityCreated(CallbackAfterEntityCreated &cb) {
    return pushCallbackByComponents<Components...>(cb, 0);
  }
  // Register a callback function to be called right before an entity associated with given
  // components is removed.
  template <typename... Components> uint32_t BeforeEntityRemoved(CallbackBeforeEntityRemoved &cb) {
    return pushCallbackByComponents<Components...>(cb, 1);
  }
  // Remove callback from management by callback id.
  void RemoveCallback(uint32_t id);
  inline size_t NumCallbacks() const { return callbacks.size(); }
  // Apply delayed entity killings for all archetypes.
  void ApplyDelayedKills();
  // Apply delayed entity creations for all archetypes.
  void ApplyDelayedNewEntities();

protected:
  // Impls IWorld
  void afterEntityCreated(ArchetypeId a, EntityShortId e) override { triggerCallbacks(a, e, 0); }
  void beforeEntityRemoved(ArchetypeId a, EntityShortId e) override { triggerCallbacks(a, e, 1); }
  void addDelayedNewEntity(ArchetypeId a, EntityShortId e) override {
    toBorn.push_back(__internal::pack(a, e));
  }
  void addDelayedKillEntity(ArchetypeId a, EntityShortId e) override {
    toKill.push_back(__internal::pack(a, e));
  }

private:
  std::vector<std::unique_ptr<__internal::IArchetype>> archetypes;
  std::unique_ptr<__internal::Matcher> matcher;

  // We should respect to the order of DelayedXXX calls, in case of possible entity dependency relations.
  std::deque<EntityId> toBorn, toKill;

  /// ~~~~~~~~~~ Callback ~~~~~~~~~~~~~~~~~
  struct Callback {
    using Func = __internal::CallbackEntityLifecycle;
    uint32_t id = 0;
    int flag; // 0: AfterEntityCreated, 1: BeforeEntityRemoved
    Func func;
    const __internal::AIdsPtr aids; // stored in matcher.
  };
  uint32_t nextCallbackId = 0;
  // Key is callback's id
  std::unordered_map<uint32_t, std::unique_ptr<Callback>> callbacks;
  // Callback table's pointers are stored continuously in memory for an archetype and certain flag.
  // Format: callbackTable[flag][archetype id] => callbacks ponters.
  // Purpose: redundancy for performant triggering callbacks
  std::vector<std::vector<const Callback *>> callbackTable[2];

  uint32_t pushCallback(int flag, const __internal::AIdsPtr aids, const Callback::Func &func);
  void triggerCallbacks(ArchetypeId aid, EntityShortId e, int flag);

  template <typename... Components> uint32_t pushCallbackByComponents(const Callback::Func &func, int flag) {
    if (archetypes.empty())
      throw std::runtime_error("tinyecs: callbacks should register **after** all archetypes are created");
    using __internal::MatchRelation;
    using CS = __internal::ComponentSequence<Components...>;
    const auto aids = matcher->MatchAndStore(MatchRelation::ALL, CS::GetSignature());
    return pushCallback(flag, aids, func);
  }

  friend __internal::IQuery;  // for matcher
  friend __internal::ICacher; // for push & remove callbacks
};

namespace __internal { // DO NOT USE NAMES FROM __internal

//////////////////////////
/// Filter
//////////////////////////

// ==,!=,<,<=,>,>=,in,between [a,b]
enum class OP { EQ, NE, LT, LE, GT, GE, IN, BT };

// forward declarations
template <typename Value> class SimpleFilter;            // ==,!=
template <typename Value> class OrderedComparatorFilter; // <,<=,>,>=
template <typename Value> class InFilter;
template <typename Value> class BetweenFilter;

class IFieldIndexRoot; // forward declaration.

// CallbackOnIndexValueUpdated is a internal callback function.
// Which is called when a value of associated index is updated.
using CallbackOnIndexValueUpdated = std::function<void(IFieldIndexRoot *, EntityId)>;

class IFieldIndexRoot {
public:
  uint32_t OnIndexValueUpdated(const CallbackOnIndexValueUpdated &cb);
  inline void RemoveOnValueUpdatedCallback(uint32_t id) { callbacks.erase(id); }
  inline size_t NumCallbacks() const { return callbacks.size(); }

protected:
  void onUpdate(EntityId eid);

private:
  uint32_t nextCallbackId = 0;
  std::unordered_map<uint32_t, CallbackOnIndexValueUpdated> callbacks;
};

// type of filterXXX function's callback, to iterate each valid entity, returns true to stop.
using CallbackFilter = std::function<bool(EntityId eid)>;
template <typename Value> class IFieldIndexFilterApi : virtual public IFieldIndexRoot {
protected:
  // filterXXX functions filters by rhs, and call given callback cb for each valid entity id.
  virtual void filterEqual(CallbackFilter &cb, const Value &rhs) = 0;                  // value == rhs
  virtual void filterNonEqual(CallbackFilter &cb, const Value &rhs) = 0;               // value != rhs
  virtual void filterIn(CallbackFilter &cb, const std::unordered_set<Value> &rhs) = 0; // in { Value... }
  friend SimpleFilter<Value>;
  friend InFilter<Value>;
};

template <typename Value> class OrderedIFieldIndexFilterApi : virtual public IFieldIndexFilterApi<Value> {
protected:
  virtual void filterLessThan(CallbackFilter &cb, const Value &rhs) = 0;                  // value < rhs
  virtual void filterLessEqualThan(CallbackFilter &cb, const Value &rhs) = 0;             // value <= rhs
  virtual void filterGreaterThan(CallbackFilter &cb, const Value &rhs) = 0;               // value > rhs
  virtual void filterGreaterEqualThan(CallbackFilter &cb, const Value &rhs) = 0;          // value >= rhs
  virtual void filterBetween(CallbackFilter &cb, const std::pair<Value, Value> &rhs) = 0; // [first, send]
  friend SimpleFilter<Value>;
  friend InFilter<Value>;
  friend OrderedComparatorFilter<Value>;
  friend BetweenFilter<Value>;
};

struct IFilter {
  virtual ~IFilter() = default;
  virtual void Execute(CallbackFilter &cb) const = 0;
  virtual IFieldIndexRoot *GetIndexPtr() const = 0;
};

template <typename Value> struct SimpleFilter : IFilter {
  IFieldIndexFilterApi<Value> *idx = nullptr;
  OP op;
  const Value rhs;
  SimpleFilter(IFieldIndexFilterApi<Value> *idx, OP op, const Value &rhs) : idx(idx), op(op), rhs(rhs) {}
  inline IFieldIndexRoot *GetIndexPtr() const override { return idx; }
  void Execute(CallbackFilter &cb) const override {
    switch (op) { // clang-format off
    case OP::EQ: return idx->filterEqual(cb, rhs);
    case OP::NE: return idx->filterNonEqual(cb, rhs);
    default: return; // kill the warning: enumeration values not handled
    } // clang-format on
  }
};

template <typename Value> struct OrderedComparatorFilter : IFilter {
  OrderedIFieldIndexFilterApi<Value> *idx = nullptr;
  OP op;
  const Value rhs;
  OrderedComparatorFilter(OrderedIFieldIndexFilterApi<Value> *idx, OP op, const Value &rhs)
      : idx(idx), op(op), rhs(rhs) {}
  inline IFieldIndexRoot *GetIndexPtr() const override { return idx; }
  void Execute(CallbackFilter &cb) const override {
    switch (op) { // clang-format off
    case OP::LT: return idx->filterLessThan(cb, rhs);
    case OP::LE: return idx->filterLessEqualThan(cb, rhs);
    case OP::GT: return idx->filterGreaterThan(cb, rhs);
    case OP::GE: return idx->filterGreaterEqualThan(cb, rhs);
    default: return; // kill the warning: enumeration values not handled
    } // clang-format on
  }
};

template <typename Value> struct InFilter : IFilter {
  IFieldIndexFilterApi<Value> *idx = nullptr;
  using Rhs = std::unordered_set<Value>;
  const Rhs rhs; // keep a constant copy
  InFilter(IFieldIndexFilterApi<Value> *idx, const Rhs &rhs) : idx(idx), rhs(rhs) {}
  inline IFieldIndexRoot *GetIndexPtr() const override { return idx; }
  void Execute(CallbackFilter &cb) const override { idx->filterIn(cb, rhs); }
};

template <typename Value> struct BetweenFilter : IFilter {
  OrderedIFieldIndexFilterApi<Value> *idx = nullptr;
  using Rhs = std::pair<Value, Value>;
  const Rhs rhs;
  BetweenFilter(OrderedIFieldIndexFilterApi<Value> *idx, const Rhs &rhs) : idx(idx), rhs(rhs) {}
  inline IFieldIndexRoot *GetIndexPtr() const override { return idx; }
  void Execute(CallbackFilter &cb) const override { idx->filterBetween(cb, rhs); }
};

//////////////////////////
/// FieldIndex
//////////////////////////

// Internal base class of field index, which is designed to speed up filtering queries.
class IFieldIndex {
public:
  IFieldIndex() = default;
  virtual ~IFieldIndex() = default;
  inline void Bind(IWorld *w) { world = w; }
  inline void Bind(World &w) { world = &w; }
  bool inline IsBind() { return world != nullptr; }

protected:
  inline EntityId getWorldLastCreatedEntityId() const { return world->lastCreatedEntityId; }
  inline bool isWorldLastCreatedEntityIdSet() const {
    return world != nullptr && world->lastCreatedEntityIdSet;
  }

private:
  IWorld *world = nullptr;
};

template <typename Value> class IFieldIndexOperators : virtual public IFieldIndexFilterApi<Value> {
  using enum OP; // clang-format off
  using This = IFieldIndexOperators;
public:
  auto operator==(const Value &rhs) const { return std::make_shared<SimpleFilter<Value>>(const_cast<This *>(this), EQ, rhs); }
  auto operator!=(const Value &rhs) const { return std::make_shared<SimpleFilter<Value>>(const_cast<This *>(this), NE, rhs); }
  auto In(const std::unordered_set<Value> &rhs) const { return std::make_shared<InFilter<Value>>(const_cast<This *>(this), rhs); }
}; // clang-format on

template <typename Value>
class OrderedIFieldIndexOperators : virtual public IFieldIndexOperators<Value>,
                                    virtual public OrderedIFieldIndexFilterApi<Value> {
  using This = OrderedIFieldIndexOperators;
  using enum OP; // clang-format off
public:
  auto operator<(const Value &rhs) const { return std::make_shared<OrderedComparatorFilter<Value>>(const_cast<This *>(this), LT, rhs); }
  auto operator<=(const Value &rhs) const { return std::make_shared<OrderedComparatorFilter<Value>>(const_cast<This *>(this), LE, rhs); }
  auto operator>(const Value &rhs) const { return std::make_shared<OrderedComparatorFilter<Value>>(const_cast<This *>(this), GT, rhs); }
  auto operator>=(const Value &rhs) const { return std::make_shared<OrderedComparatorFilter<Value>>(const_cast<This *>(this), GE, rhs); }
  auto Between(const std::pair<Value, Value> &rhs) const { return std::make_shared<BetweenFilter<Value>>(const_cast<This *>(this), rhs); } // [first, second]
}; // clang-format on

// IFieldIndexImpl is the base of index implementation classes. Wehere Value is the field value's type,
// Container is the contaienr type, Iterator is the type of the Container's iterator.
template <typename Value, typename Container, typename Iterator> class IFieldIndexImpl {
public:
  // ~~~~~~~~ Overridings for subclasses to impl ~~~~~~~~~~~
  // Returns the number of elements managed in this Index.
  virtual size_t Size() const = 0;
  // Erase a single element at position provided by given iterator.
  virtual void Erase(Iterator &it) = 0;
  // Creates a new value into this index, returns the position iterator.
  // Returns end() if failure or we shouldn't insert a value to the index.
  virtual Iterator Insert(Value &v) = 0;
  // Replace the value located at position provided by given iterator.
  virtual Iterator Update(Iterator &it, Value &v) = 0;
  // Returns the end iterator.
  virtual const Iterator End() = 0;
  // Clear the content of this index.
  virtual void Clear() = 0;
};

template <typename Value, typename TMap, typename TIterator = TMap::iterator>
class MapBasedFieldIndex
    : virtual public __internal::IFieldIndexFilterApi<Value>, // virtual inherit root: IFieldIndexRoot
      public __internal::IFieldIndex,
      public __internal::IFieldIndexImpl<Value, TMap, TIterator> {
public:
  using Container = TMap;
  using Iterator = TIterator;

  // ~~~~~~~ Impl IFieldIndexImpl ~~~~~~~~~~~
  inline size_t Size() const override { return m.size(); }
  void Erase(Iterator &it) override { m.erase(it); }
  Iterator Insert(Value &v) override {
    // We shouldn't insert a value if this entity isn't bound to the world.
    if (!this->isWorldLastCreatedEntityIdSet()) return m.end();
    return m.insert({v, this->getWorldLastCreatedEntityId()});
  }
  Iterator Update(Iterator &it, Value &v) override {
    auto eid = it->second;
    m.erase(it);
    auto it1 = m.insert({v, eid});
    IFieldIndexRoot::onUpdate(eid);
    return it1;
  }
  const Iterator End() override { return m.end(); }
  void Clear() override { m.clear(); }

protected:
  Container m;

  // ~~~~~~~~ Impl IFieldIndexFilterApi ~~~~~~~~~~~~~
  void filterEqual(CallbackFilter &cb, const Value &rhs) override { // value == rhs
    // begin ----------[ rhs rhs rhs ] *----- end
    //                   |    <ans>    |
    //                  lower         upper
    __filterEqual(cb, rhs);
  }
  void filterNonEqual(CallbackFilter &cb, const Value &rhs) override { // value != rhs
    // begin ----------] rhs rhs rhs [ *----- end
    //          <ans1>   |             | <ans2>
    //                  lower         upper
    auto [lower, upper] = m.equal_range(rhs);
    for (auto it = m.begin(); it != lower; ++it)
      if (cb(it->second)) return;
    for (auto it = upper; it != m.end(); ++it)
      if (cb(it->second)) return;
  }
  void filterIn(CallbackFilter &cb,
                const std::unordered_set<Value> &rhs) override { // in { Value... }

    for (const auto &v : rhs)          // TODO: any optimization?
      if (__filterEqual(cb, v)) break; // cppcheck-suppress useStlAlgorithm
  }

private:
  bool __filterEqual(const CallbackFilter &cb, const Value &rhs) {
    auto [lower, upper] = m.equal_range(rhs);
    for (auto it = lower; it != upper; ++it)
      if (cb(it->second)) return true; // earlier break
    return false;
  }
};

} // namespace __internal

// UnorderedFieldIndex is an unordered_ index (hash) index, based on std::unordered_multimap.
// Time complexity: Erase, Insert, Update => O(1)
template <typename Value>
class UnorderedFieldIndex // virtual inherit root: IFieldIndexRoot
    : virtual public __internal::IFieldIndexOperators<Value>,
      virtual public __internal::MapBasedFieldIndex<
          Value, std::unordered_multimap<Value, EntityId>,
          typename std::unordered_multimap<Value, EntityId>::iterator> {};

// OrderedFieldIndex is an ordered index based on std::multimap.
// Time complexity: Erase, Insert, Update => O(logN)
template <typename Value>
class OrderedFieldIndex // virtual inherit root: IFieldIndexRoot
    : virtual public __internal::OrderedIFieldIndexFilterApi<Value>,
      virtual public __internal::OrderedIFieldIndexOperators<Value>,
      virtual public __internal::MapBasedFieldIndex<Value, std::multimap<Value, EntityId>,
                                                    typename std::multimap<Value, EntityId>::iterator> {
  using CallbackFilter = __internal::CallbackFilter;

protected:
  // ~~~~~~~~ Impl IFieldIndexFilterApi ~~~~~~~~~~~~~
  void filterLessThan(CallbackFilter &cb, const Value &rhs) override { // v < rhs
    // begin ----------] rhs rhs rhs ------ end
    //         <ans>      |
    //                  lower_bound
    auto bound = this->m.lower_bound(rhs);
    for (auto it = this->m.begin(); it != bound; ++it)
      if (cb(it->second)) break;
  }
  void filterLessEqualThan(CallbackFilter &cb, const Value &rhs) override { // v <= rhs
    // begin ------ rhs rhs rhs ] *--------- end
    //           <ans>            |
    //                        upper_bound
    auto bound = this->m.upper_bound(rhs);
    for (auto it = this->m.begin(); it != bound; ++it)
      if (cb(it->second)) break;
  }
  void filterGreaterThan(CallbackFilter &cb, const Value &rhs) override { // v > rhs
    // begin ------ rhs rhs rhs *--------- end
    //                          |    <ans>
    //                        upper_bound
    for (auto it = this->m.upper_bound(rhs); it != this->m.end(); ++it)
      if (cb(it->second)) break;
  }
  void filterGreaterEqualThan(CallbackFilter &cb, const Value &rhs) override { // v >= rhs
    // begin -----[ rhs rhs ---------- end
    //              |     <ans>
    //            lower_bound
    for (auto it = this->m.lower_bound(rhs); it != this->m.end(); ++it)
      if (cb(it->second)) break;
  }
  void filterBetween(CallbackFilter &cb, const std::pair<Value, Value> &rhs) override { // [first, second]
    // begin -----[ f f ---- s s ] *------ end
    //              |    <ans>     |
    //            lower          upper
    auto lower = this->m.lower_bound(rhs.first), upper = this->m.upper_bound(rhs.second);
    for (auto it = lower; it != upper; ++it)
      if (cb(it->second)) break;
  }
};

//////////////////////////
/// FieldProxy
//////////////////////////

namespace __internal {

// FieldProxy wraps a component field to be used as a query index.
template <typename Value, typename TFieldIndex> class FieldProxyBase {
public:
  FieldProxyBase() = default;
  FieldProxyBase(const Value &value) // cppcheck-suppress noExplicitConstructor
      : value(value) {}              // copy
  FieldProxyBase(const FieldProxyBase &o) : FieldProxyBase(o.value) {}
  ~FieldProxyBase() {
    if (__index != nullptr && __it != __index->End()) __index->Erase(__it);
  }
  inline const Value &GetValue() const { return value; }
  // ~~~~ API Hooks ~~~~
  // Inserts the initial value on entity construction.
  // If call BindIndex on a field for multiple times, only the first call takes effect.
  // This function must be called inside a Component's constructor if it declares
  // a field wrapped by FieldProxy.
  void BindIndex(TFieldIndex *idx) {
    if (idx == nullptr) throw std::runtime_error("tinyecs: cannot bind nullptr index to field");
    if (__index != nullptr) return; // idempotent
    __index = idx;
    __it = __index->Insert(value); // __it may be end()
  }
  inline void BindIndex(TFieldIndex &idx) { BindIndex(&idx); }
  inline bool IsBind() const { return __index != nullptr && __it != __index->End(); }
  /// ~~~~ Operators ~~~~
  FieldProxyBase &operator=(const Value &v) { return set(v); }
  FieldProxyBase &operator=(const FieldProxyBase &o) { return *this = o.value; }
  bool operator==(const Value &v) { return value == v; }
  bool operator==(const FieldProxyBase &o) { return value == o.value; }
  bool operator!=(const Value &v) { return !(*this == v); }
  bool operator!=(const FieldProxyBase &o) { return !(*this == o); }

protected:
  Value value;
  TFieldIndex *__index = nullptr;
  // iterator to trackpostion in __index.
  // It may be a iterator `end()`, which indicates this entity is unbound with some world.
  // In such situations, we shouldn't insert/updates its value to the index, and the index will neither filter
  // it out on queries.
  typename TFieldIndex::Iterator __it;
  FieldProxyBase &set(const Value &v) {
    if (__index == nullptr) throw std::runtime_error("tinyecs: FieldProxy's BindIndex function may not run");
    // this entity is unbounded with the world, we shouldn't update it.
    if (__it == __index->End()) return *this;
    value = v, __it = __index->Update(__it, value);
    return *this;
  }
};

template <typename Value, typename TFieldIndex>
bool operator==(const Value &v, const FieldProxyBase<Value, TFieldIndex> &fd) { // 5 == field
  return v == fd.GetValue();
}
template <typename Value, typename TFieldIndex>
bool operator!=(const Value &v, const FieldProxyBase<Value, TFieldIndex> &fd) { // 5 != field
  return v != fd.GetValue();
}
} // namespace __internal

template <typename Value, typename TFieldIndex> // Normal FieldProxy
struct FieldProxy : public __internal::FieldProxyBase<Value, TFieldIndex> {
  using __internal::FieldProxyBase<Value, TFieldIndex>::FieldProxyBase;
};

template <std::integral Value, typename TFieldIndex> // Integral FieldProxy
struct FieldProxy<Value, TFieldIndex> : public __internal::FieldProxyBase<Value, TFieldIndex> {
  using __internal::FieldProxyBase<Value, TFieldIndex>::FieldProxyBase;
  operator bool() const { return this->value; }
  bool operator!() { return !(this->value); }
  bool operator<(const Value &v) const { return this->value < v; }
  bool operator<(const FieldProxy &o) const { return this->value < o.value; }
  bool operator<=(const Value &v) const { return this->value <= v; }
  bool operator<=(const FieldProxy &o) const { return this->value <= o.value; }
  bool operator>(const Value &v) const { return this->value > v; }
  bool operator>(const FieldProxy &o) const { return this->value > o.value; }
  bool operator>=(const Value &v) const { return this->value >= v; }
  bool operator>=(const FieldProxy &o) const { return this->value >= o.value; }
  FieldProxy &operator+=(const Value &v) { return *this = this->value + v; }
  FieldProxy &operator+=(const FieldProxy &o) { return *this += o.value; }
  FieldProxy &operator-=(const Value &v) { return *this = this->value - v; }
  FieldProxy &operator-=(const FieldProxy &o) { return *this -= o.value; }
  FieldProxy &operator*=(const Value &v) { return *this = this->value * v; }
  FieldProxy &operator*=(const FieldProxy &o) { return *this *= o.value; }
  FieldProxy &operator/=(const Value &v) { return *this = this->value / v; }
  FieldProxy &operator/=(const FieldProxy &o) { return *this /= o.value; }
  FieldProxy &operator%=(const Value &v) { return *this = this->value % v; }
  FieldProxy &operator%=(const FieldProxy &o) { return *this %= o.value; }
  FieldProxy &operator|=(const Value &v) { return *this = this->value | v; }
  FieldProxy &operator|=(const FieldProxy &o) { return *this |= o.value; }
  FieldProxy &operator&=(const Value &v) { return *this = this->value & v; }
  FieldProxy &operator&=(const FieldProxy &o) { return *this &= o.value; }
  FieldProxy &operator^=(const Value &v) { return *this = this->value ^ v; }
  FieldProxy &operator^=(const FieldProxy &o) { return *this ^= o.value; }
};

template <std::integral Value, typename TFieldIndex>
bool operator<(const Value &v, const FieldProxy<Value, TFieldIndex> &fd) { // 5 < field
  return v < fd.GetValue();
}
template <std::integral Value, typename TFieldIndex>
bool operator<=(const Value &v, const FieldProxy<Value, TFieldIndex> &fd) { // 5 <= field
  return v <= fd.GetValue();
}
template <std::integral Value, typename TFieldIndex>
bool operator>(const Value &v, const FieldProxy<Value, TFieldIndex> &fd) { // 5 > field
  return v > fd.GetValue();
}
template <std::integral Value, typename TFieldIndex>
bool operator>=(const Value &v, const FieldProxy<Value, TFieldIndex> &fd) { // 5 >= field
  return v >= fd.GetValue();
}

namespace __internal {
template <class T>
concept String = std::is_convertible_v<T, std::string>;
} // namespace __internal

template <__internal::String Value, typename TFieldIndex> // String FieldProxy
struct FieldProxy<Value, TFieldIndex> : public __internal::FieldProxyBase<Value, TFieldIndex> {
  using Base = __internal::FieldProxyBase<Value, TFieldIndex>;
  using Base::FieldProxyBase;
  template <size_t N>
  FieldProxy(const char (&s)[N]) : Base(Value(s)) {} // cppcheck-suppress noExplicitConstructor
  FieldProxy(std::string_view s) : Base(Value(s)) {} // cppcheck-suppress noExplicitConstructor
  FieldProxy &operator+=(const Value &v) { return *this = this->value + v; } // s += std::string("abc")
  FieldProxy &operator+=(const FieldProxy &o) { return *this += o.value; }   // s += other.s
  FieldProxy &operator+=(std::string_view sv) { return *this = this->value + Value(sv); } // s += string_view
  template <size_t N> FieldProxy &operator+=(const char (&s)[N]) {
    return *this = this->value + s;
  } // s += "abc"
};

// Type of a vector constant filter pointers.
using Filter = std::shared_ptr<const __internal::IFilter>;
using Filters = std::vector<Filter>;

//////////////////////////
/// Cacher
//////////////////////////

namespace __internal { // DO NOT USE NAMES FROM __internal

class ICacher {
public:
public:
  ICacher(World &world, const std::unordered_map<ArchetypeId, IArchetype *> &archetypes, const AIdsPtr aids,
          const Filters &filters)
      : world(world), archetypes(archetypes), aids(aids), filters(filters) {}
  ~ICacher() { clearCallbacks(); }

  // Cache iteration ForEach methods (in-place iteration).
  // The order of iteration is according to the entity ids from small to large,
  // and entities in the same archetype will be accessed next to each other.
  // It's **undefined behavior** to create/remove entities, or update indexes associated with this
  // cacher's in given callback. For such situations, consider to create and remove entities at frame
  // begin or end, or just use Collect() to safely work on a copy.
  void ForEach(const Accessor &cb);
  inline void ForEach(const Accessor &&cb) { ForEach(cb); }
  inline void ForEachUntil(const AccessorUntil &cb) { forEachUntil(cb); }
  inline void ForEachUntil(const AccessorUntil &&cb) { forEachUntil(cb); }

  // Copy the cached entity references into given vector.
  void Collect(std::vector<EntityReference> &vec);

protected:
  void setup(IQuery &q);
  // Virtual methods for following Cache class.
  virtual void insert(EntityId eid, const EntityReference &ref) = 0;
  virtual void erase(EntityId eid) = 0;
  virtual void forEachUntil(const AccessorUntil &cb) = 0;

private:
  //~~~~~~ content from the Query ~~~~~~
  World &world;
  const AIdsPtr aids; // stored in matcher.
  const std::unordered_map<ArchetypeId, IArchetype *> archetypes;
  const Filters filters; // optional empty
  // ecbs: callbacks for cache, watching entities create/remove.
  std::vector<uint32_t> ecbs;
  // ucbs: callbacks for cache watching index updates.
  std::vector<std::pair<IFieldIndexRoot *, uint32_t>> icbs;

  void setupCallbacksWatchingEntities();
  void setupCallbacksWatchingIndexes();
  void clearCallbacks();
};

// Cacher caches results from a query and automatically watching changes.
// For instance, if an entity is created and it matches the cache's interested components
// and filters, then it will be added to the internal cache automatically. And the same
// mechanism for entity removes.
using DefaultCacherCompare = std::less<EntityId>;
template <typename Compare = DefaultCacherCompare> class Cacher : public ICacher {
  // Use a map instead of unordered_map, reason:
  // We should respect the continuity of entities in memory when iterating over the cache.
  // Entities in the same archetype are consecutive after sorting their ids.
  // The drawbacks here is: maintaining cache time complexity O(logN), where N is the cache size.
  // The time iterating the cache is not affected.
  using Container = std::map<EntityId, EntityReference, Compare>;

public:
  Cacher(IQuery &q, World &world, const std::unordered_map<ArchetypeId, IArchetype *> &archetypes,
         const AIdsPtr aids, const Filters &filters)
      : ICacher(world, archetypes, aids, filters) {
    setup(q);
  }
  Cacher(IQuery &q, World &world, const std::unordered_map<ArchetypeId, IArchetype *> &archetypes,
         const AIdsPtr aids, const Filters &filters, Compare cmp)
      : ICacher(world, archetypes, aids, filters), cache(Container(cmp)) {
    setup(q);
  }
  ~Cacher() = default;
  // Cacher is stateful, we cannot copy it.
  Cacher(const Cacher &o) = delete;            // disable copy constructor
  Cacher &operator=(const Cacher &o) = delete; // disable copy assign
  // Iterators of internal container for for-based iteration.
  inline auto begin() { return cache.begin(); }
  inline auto end() { return cache.end(); }

protected:
  void insert(EntityId eid, const EntityReference &ref) override { cache.insert({eid, ref}); }
  void erase(EntityId eid) override { cache.erase(eid); }
  void forEachUntil(const AccessorUntil &cb) override {
    for (auto &[_, ref] : cache)
      if (cb(ref)) break;
  }

private:
  Container cache;
};

//////////////////////////
/// Query
//////////////////////////

// internal Query base class.
class IQuery {
public:
  explicit IQuery(World &world, const MatchRelation relation, const Signature &signature)
      : world(world), relation(relation), signature(signature) {}
  ~IQuery() = default;

  // PreMatch should be called at the system setup stage, after all archetypes are created.
  // Executes a query without PreMatch called will throw a runtime_error.
  // Uses a standalone method instead of pre-match inside the constructor is more explicit.
  IQuery &PreMatch();

  // Append filters, it's important to know that the left a filter is,
  // the earlier it will be executed, so the filter with higher
  // distinction should be placed firstly.
  IQuery &Where(const Filter &f);   // copy one
  IQuery &Where(Filter &&f);        // move one
  IQuery &Where(const Filters &fl); // copy multiple
  IQuery &Where(Filters &&fl);      // move multiple
  // Clears filters.
  IQuery &ClearFilters();

  // Execute the query, and call given callback for each matched entities **in place**.
  // The order of iteration is according to the entity ids from small to large,
  // and entities in the same archetype will be accessed next to each other.
  // Internal brief:
  // 1. If filters are provided, apply filters and then iterates matched entities for each archetype.
  // 2. Otherwise, this is a simple query just about some archetypes, for each archetype, run with
  //    all entities managed by archetypes.
  // It's **undefined behavior** if the callback contains logics that creates or removes entities.
  // For such situations, consider to create and remove entities at frame begin or end, or just
  // use Collect() to safely work on a copy.
  void ForEach(const Accessor &cb);
  inline void ForEach(const Accessor &&cb) { ForEach(cb); }
  void ForEachUntil(const AccessorUntil &cb);
  inline void ForEachUntil(const AccessorUntil &&cb) { ForEachUntil(cb); }

  // Execute the query, and copy entity reference results to given vector.
  // The order of collected entities is arranged from small to large by entity id.
  void Collect(std::vector<EntityReference> &vec);

  // Constructs a cache from this query, this will execute the query at once and
  // changes will be maintained in the cache automatically.
  [[nodiscard]] inline Cacher<> Cache() { return Cacher(*this, world, archetypes, aids, filters); }
  // Constructs a cache from this query, with a custom compare function.
  template <typename Compare> [[nodiscard]] inline Cacher<Compare> Cache(Compare cmp) {
    return Cacher<Compare>(*this, world, archetypes, aids, filters, cmp);
  }

protected:
  World &world;
  bool ready = false;
  AIdsPtr aids;
  std::unordered_map<ArchetypeId, IArchetype *> archetypes; // redundancy
  const Signature &signature;                               // ref to static storage.
  const MatchRelation relation;
  Filters filters;

  void executeWithFilters(const AccessorUntil &cb);
  void executeForAll(const AccessorUntil &cb);
};

template <MatchRelation Relation, typename... Components> class QueryImpl : public IQuery {
  using CS = __internal::ComponentSequence<Components...>;

public:
  static_assert(!(Relation == MatchRelation::ALL && CS::N == 0),
                "tinyecs: Query requires at least one component type parameter");
  static_assert(!(Relation == MatchRelation::NONE && CS::N == 0),
                "tinyecs: QueryNone requires at least one component type parameter");
  explicit QueryImpl(World &world) : IQuery(world, Relation, CS::GetSignature()) {}
  explicit QueryImpl(World &world, const Filters &fl) : IQuery(world, Relation, CS::GetSignature()) {
    filters = fl; // copy
  }
  explicit QueryImpl(World &world, Filters &&fl) : IQuery(world, Relation, CS::GetSignature()) {
    filters.swap(fl); // move
  }
};

} // namespace __internal

// Query entities matching all of given list of components.
template <typename... Components>
using Query = __internal::QueryImpl<__internal::MatchRelation::ALL, Components...>;

// Query entities matching any of given list of components.
// Use QueryAny<> to query arbitrary components, to iterate every entity in the world.
template <typename... Components>
using QueryAny = __internal::QueryImpl<__internal::MatchRelation::ANY, Components...>;

// Query entities matching none of given list of components.
template <typename... Components>
using QueryNone = __internal::QueryImpl<__internal::MatchRelation::NONE, Components...>;

} // namespace tinyecs

#endif
