// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/tinyecs
// Requirements: at least C++20.

#include "tinyecs.h"

namespace tinyecs {

namespace __internal {

ComponentId IComponentBase::nextId = 0;
static EntityReference NullEntityReference = {};

/////////////////////////
/// IArchetype
/////////////////////////

// Internal dev notes: Now a not so good solution is to temporarily store the ID of
// the entity being created in the entire world before and after calling the constructor
// of components during the creation of an entity, so that the index can initialize the
// fieldproxy's value of this entity.
// Progess demo:
//   IArchetype -> setLastCreatedEntityId()
//   IArchetype -> call constructors of entity's components.
//                 -> FieldProxy's constructor called
//                    -> get lastCreatedEntityId and insert to its index.
//   IArchetype -> clearLastCreatedEntityId()
void IWorld::setLastCreatedEntityId(EntityId eid) {
  lastCreatedEntityId = eid;
  lastCreatedEntityIdSet = true;
}

IArchetype::IArchetype(ArchetypeId id, IWorld *world, size_t numCols, size_t cellSize,
                       const Signature &signature)
    : id(id), world(world), numCols(numCols), cellSize(cellSize), blockSize(numRows * numCols * cellSize),
      signature(signature) {
  // 0xffff means a invalid column.
  std::fill_n(cols, MaxNumComponents, 0xffff);
  int col = 0;
  for (int i = 0; i < MaxNumComponents; i++)
    if (signature[i]) cols[i] = col++;
}

unsigned char *IArchetype::getEntityData(EntityShortId e) const {
  auto block = blocks[e / numRows].get();
  return block + (e % numRows) * numCols * cellSize;
}

void IArchetype::get(EntityReference &ref, EntityShortId e) {
  if (!contains(e)) {
    ref = NullEntityReference;
    return;
  }
  uncheckedGet(ref, e);
}

unsigned char *IArchetype::getComponentRawPtr(unsigned char *entityData, ComponentId cid) const {
  auto col = cols[cid];
  if (col == 0xffff)
    throw std::runtime_error("tinyecs: component of archetype " + std::to_string(id) + " not found");
  return entityData + col * cellSize;
}

// Creates a new entity, if a reference pointer provided, fill the data inplace into it.
// If cemetery contains dead entity ids, recycle it at first.
// Otherwise, increase the cursor to occupy a new entity short id and data row.
// If the data row exceeds the total size of blocks, then creates a new block.
EntityId IArchetype::newEntity(EntityReference *ref) {
  EntityShortId e;
  unsigned char *data;
  if (cemetery.size()) { // Recycle
    // Possible improvements todo: use ordered set for cemetery,
    // to recycle oldest deaded firstly.
    e = *cemetery.begin();
    cemetery.erase(e);
    data = getEntityData(e);
    std::fill_n(data, numCols * cellSize, 0);
  } else { // Increase
    e = ecursor++;
    if (e >= numRows * blocks.size()) { // Allocate block if not enough
      blocks.push_back(std::make_unique<unsigned char[]>(blockSize));
      std::fill_n(blocks.back().get(), blockSize, 0);
    }
    data = getEntityData(e);
  }
  // Call constructors
  auto eid = pack(id, e);
  world->setLastCreatedEntityId(eid);
  constructComponents(data);
  world->clearLastCreatedEntityId();
  world->afterEntityCreated(id, e); // after created
  if (ref != nullptr) ref->reset(this, data, eid);
  return eid;
}

void IArchetype::remove(EntityShortId e) {
  if (e >= ecursor) return;
  auto data = getEntityData(e);
  world->beforeEntityRemoved(id, e); // before removing
  destructComponents(data);
  cemetery.insert(e);
}

void IArchetype::ForEach(const Accessor &cb) {
  ForEachUntil([&cb](EntityReference &ref) {
    cb(ref);
    return false;
  });
}

void IArchetype::ForEachUntil(const AccessorUntil &cb) {
  // It's important to scan entities in order of address layout.
  EntityReference ref(const_cast<IArchetype *>(this));
  for (EntityShortId e = 0; e < ecursor; e++)
    if (!cemetery.contains(e)) {
      uncheckedGet(ref, e);
      if (cb(ref)) break;
    }
}

void IArchetype::ForEachUntil(const AccessorUntil &cb, const OrderedEntityShortIdSet &st) {
  EntityReference ref(const_cast<IArchetype *>(this));
  for (auto e : st) {
    uncheckedGet(ref, e);
    if (cb(ref)) break;
  }
}

//////////////////////////
/// Matcher
//////////////////////////

void Matcher::PutArchetypeId(const Signature &signature, ArchetypeId aid) {
  all[aid] = 1;
  for (int i = 0; i < MaxNumComponents; i++)
    if (signature[i]) b[i].set(aid);
}

const AIdsPtr Matcher::MatchAndStore(MatchRelation relation, const Signature &signature) {
  auto ans = Match(relation, signature);
  auto ptr = std::make_shared<AIds>(std::move(ans));
  store.push_back(ptr);
  return ptr;
}

AIds Matcher::Match(MatchRelation relation, const Signature &signature) const {
  ArchetypeIdBitset ans;
  switch (relation) {
  case MatchRelation::ALL:
    ans = matchAll(signature);
    break;
  case MatchRelation::ANY:
    if (signature.none()) ans = all;
    else
      ans = matchAny(signature);
    break;
  case MatchRelation::NONE:
    ans = matchNone(signature);
    break;
  }
  AIds results;
  for (int i = 0; i < MaxNumArchetypesPerWorld; i++)
    if (ans[i]) results.insert(i);
  return results;
}

Matcher::ArchetypeIdBitset Matcher::matchAll(const Signature &signature) const {
  ArchetypeIdBitset ans = all;
  for (int i = 0; i < MaxNumComponents; i++)
    if (signature[i]) ans &= b[i];
  return ans;
}

Matcher::ArchetypeIdBitset Matcher::matchAny(const Signature &signature) const {
  ArchetypeIdBitset ans;
  for (int i = 0; i < MaxNumComponents; i++)
    if (signature[i]) ans |= b[i];
  return ans;
}

Matcher::ArchetypeIdBitset Matcher::matchNone(const Signature &signature) const {
  return all & ~matchAny(signature);
}

} // namespace __internal

//////////////////////////
/// World
//////////////////////////

bool World::IsAlive(EntityId eid) const {
  auto aid = __internal::unpack_x(eid);
  if (aid >= archetypes.size()) return false;
  return archetypes[aid]->contains(__internal::unpack_y(eid));
}

void World::Kill(EntityId eid) {
  auto aid = __internal::unpack_x(eid);
  if (aid >= archetypes.size()) return;
  archetypes[aid]->remove(__internal::unpack_y(eid));
}

void World::get(EntityId eid, EntityReference &ref) const {
  auto aid = __internal::unpack_x(eid);
  if (aid >= archetypes.size()) {
    ref = __internal::NullEntityReference;
    return;
  }
  auto &a = archetypes[aid];
  a->get(ref, __internal::unpack_y(eid));
}

void World::uncheckedGet(EntityId eid, EntityReference &ref) const {
  auto aid = __internal::unpack_x(eid);
  auto &a = archetypes[aid];
  a->uncheckedGet(ref, __internal::unpack_y(eid));
}

void World::Get(EntityId eid, const Accessor &cb) {
  EntityReference ref;
  get(eid, ref);
  cb(ref);
}

void World::UncheckedGet(EntityId eid, const Accessor &cb) {
  EntityReference ref;
  uncheckedGet(eid, ref);
  cb(ref);
}

EntityReference World::Get(EntityId eid) const {
  EntityReference ref;
  get(eid, ref);
  return ref;
}

EntityReference World::UncheckedGet(EntityId eid) const {
  EntityReference ref;
  uncheckedGet(eid, ref);
  return ref;
}

void World::RemoveCallback(uint32_t id) {
  auto it = callbacks.find(id);
  if (it == callbacks.end()) return;
  auto &cb = it->second;
  for (const auto aid : *(cb->aids))
    std::erase_if(callbackTable[cb->flag][aid], [&cb](auto x) { return x == cb.get(); });
  callbacks.erase(it);
}

// Push a callback function into management to subscribe create and remove enntities events
// inside any of given archetypes.
uint32_t World::pushCallback(int flag, const __internal::AIdsPtr aids, const Callback::Func &func) {
  uint32_t id = nextCallbackId++; // TODO: handle overflow?
  auto cb = std::make_unique<Callback>(id, flag, func, aids);
  for (const auto aid : *aids)
    callbackTable[flag][aid].push_back(cb.get());
  callbacks[id] = std::move(cb);
  return id;
}

void World::triggerCallbacks(ArchetypeId aid, EntityShortId e, int flag) {
  EntityReference ref;
  for (const auto *cb : callbackTable[flag][aid]) {
    auto a = archetypes[aid].get();
    a->uncheckedGet(ref, e);
    cb->func(ref);
  }
}

//////////////////////////
/// Filter
//////////////////////////

namespace __internal {

uint32_t IFieldIndexRoot::OnIndexValueUpdated(const CallbackOnIndexValueUpdated &cb) {
  uint32_t id = nextCallbackId++;
  callbacks[id] = cb;
  return id;
}

void IFieldIndexRoot::onUpdate(EntityId eid) {
  for (auto &[_, cb] : callbacks)
    cb(this, eid);
}

//////////////////////////
/// Query
//////////////////////////

// ~~~~~~~~~~ IQuery Filters ~~~~~~~~~~~~~~~~~~~

// Apply filters starting from a position to given set of entity ids ans.
// This will remove not-satisfied entity ids from the set inplace.
void applyFiltersFrom(const Filters &filters, EntityIdSet &ans, size_t start = 0) {
  // cb fills {ans & filter} into set tmp.
  EntityIdSet tmp;
  CallbackFilter cb = [&ans, &tmp](EntityId eid) {
    if (ans.contains(eid)) tmp.insert(eid);
    return tmp.size() == ans.size(); // stop if all satisfied.
  };
  for (size_t i = start; i < filters.size() && ans.size(); i++) { // stop once ans is empty
    filters[i]->Execute(cb);
    ans.swap(tmp), tmp.clear();
  }
}

// Test given filters on a single entity, returns true if the entity satisfy all filters.
bool testFiltersSingleEntityId(const Filters &filtrers, EntityId eid) {
  EntityIdSet ans{eid};
  applyFiltersFrom(filtrers, ans, 0);
  return !ans.empty();
}

// Apply all given filters to given entity id set.
// It firstly collects all entity ids matching the first filter via initialCollector.
// And then run remaining filters to shrink the initial set inplace.
void applyFilters(const Filters &filters, EntityIdSet &ans, CallbackFilter &initialCollector) {
  filters[0]->Execute(initialCollector);
  applyFiltersFrom(filters, ans, 1);
}

inline void applyFilters(const Filters &filters, EntityIdSet &ans, CallbackFilter &&initialCollector) {
  applyFilters(filters, ans, initialCollector);
}

// ~~~~~~~~~~ IQuery Internals ~~~~~~~~~~~~~~~~~~~

// Match archetypes.
IQuery &IQuery::PreMatch() {
  if (ready) return *this;
  if (world.archetypes.empty())
    throw std::runtime_error("tinyecs: query PreMatch must be called after archetypes are all created");
  aids = world.matcher->MatchAndStore(relation, signature);
  // redundancy pointers of matched archetypes
  for (const auto &aid : *aids)
    archetypes[aid] = world.archetypes[aid].get();
  ready = true;
  return *this;
}

// Executes given callback with filters.
// Internal notes:
// We use an unordered_set to apply filters, instead of a ordered set.
// And then copy to sorted sets groupping by archetype ids.
// Reason: expecting set ans is smaller after filters applied,
// this reduces item count to sort.
void IQuery::executeWithFilters(const AccessorUntil &cb) {
  // Filter matched entities from indexes.
  EntityIdSet ans;
  applyFilters(filters, ans, [&ans, this](EntityId eid) {
    // Must belong to interested archetypes.
    if (archetypes.contains(unpack_x(eid))) ans.insert(eid);
    return false;
  });

  // Group entity short ids by archetypes, and sort:
  // thus scanning in an archetype are as continuous in memory as possible.
  // TODO: any optimization ideas here to avoid sorting?
  std::unordered_map<ArchetypeId, OrderedEntityShortIdSet> m;
  for (auto eid : ans)
    m[__internal::unpack_x(eid)].insert(__internal::unpack_y(eid));

  // Run callback function for each archetype and matched entities inside it.
  // Note: No need to check whether an entity belongs to this query's archetypes.
  // Because the function initial collector of the first filter guarantees it.
  for (const auto &[aid, st] : m)
    archetypes[aid]->ForEachUntil(cb, st);
}

// Executes given callback directly on each interested archetypes.
void IQuery::executeForAll(const AccessorUntil &cb) {
  for (auto [_, a] : archetypes)
    a->ForEachUntil(cb);
}

// ~~~~~~~~~~ IQuery API ~~~~~~~~~~~~~~~~~~~

// Push a single filter into query's management (copy).
IQuery &IQuery::Where(const Filter &f) {
  filters.push_back(f);
  return *this;
}

// Push a single filter into query's management (move).
IQuery &IQuery::Where(Filter &&f) {
  filters.push_back(std::move(f));
  return *this;
}

// Push a list of filters into query's management (copy).
IQuery &IQuery::Where(const Filters &fl) {
  for (const auto &f : fl)
    filters.push_back(f);
  return *this;
}

// Push a list of filters into query's management (move).
IQuery &IQuery::Where(Filters &&fl) {
  for (auto &&f : fl)
    filters.push_back(std::move(f));
  return *this;
}

IQuery &IQuery::ClearFilters() {
  filters.clear();
  return *this;
}

// Iterates each matched entities.
void IQuery::ForEach(const Accessor &cb) {
  ForEachUntil([&cb](EntityReference &ref) {
    cb(ref);
    return false;
  });
}

// ForEachUntil is the ForEach that will stop once cb returns true.
void IQuery::ForEachUntil(const AccessorUntil &cb) {
  if (!ready) throw std::runtime_error("tinyecs: Query PreMatch not called");
  if (archetypes.empty()) return; // early quit.
  if (filters.empty()) return executeForAll(cb);
  executeWithFilters(cb);
}

//////////////////////////
/// Cacher
//////////////////////////

// Executes the query at once and cache them into cache container.
// Then setup callbacks to watch changes.
void ICacher::setup(__internal::IQuery &q) {
  if (aids->empty()) return; // early quit.
  // Cache entities right now (copy)
  q.ForEach([this](const EntityReference &ref) { this->insert(ref.GetId(), ref); });
  // Setup callbacks.
  setupCallbacksWatchingEntities();
  setupCallbacksWatchingIndexes();
}

// Setup callbacks to watch entities in interested archetypes.
// OnEntityCreated: add into the cache if match.
// OnEntityRemoved: remove from cache.
void ICacher::setupCallbacksWatchingEntities() {
  CallbackAfterEntityCreated onEntityCreated = [this](const EntityReference &ref) {
    // Checks if this entity satisfies all filters if presents.
    if (filters.size() && !testFiltersSingleEntityId(filters, ref.GetId())) return;
    // Note: there's no need to recheck whether the entity is inside the query's interested archetypes.
    // Because we subscribe callbacks by corresponding archetype ids.
    this->insert(ref.GetId(), ref);
  };
  CallbackBeforeEntityRemoved onEntityRemoved = [this](const EntityReference &ref) {
    // Remove anyway, it's going to die, no need to check filters at all.
    this->erase(ref.GetId());
  };
  ecbs.push_back(world.pushCallback(0, aids, onEntityCreated));
  ecbs.push_back(world.pushCallback(1, aids, onEntityRemoved));
}

// Setup callbacks to watch index value updates.
// On index updated: test all filters for corresponding entity.
// No need to watch index value inserts and removes, because we
// already watch those via entitiy life cycle callbacks.
void ICacher::setupCallbacksWatchingIndexes() {
  if (filters.empty()) return;
  using Callback = __internal::CallbackOnIndexValueUpdated;
  using Index = __internal::IFieldIndexRoot;
  Callback onIndexUpdated = [this](const Index *idx, EntityId eid) {
    // Must be alive in any of our archetypes.
    ArchetypeId aid = __internal::unpack_x(eid);
    auto it = archetypes.find(aid);
    if (it == archetypes.end()) return;

    // Test this single entity with all filters.
    // Note: we can't test the filters only associated with the updated index.
    // Because there may be some other filters dismatches this entity.
    if (testFiltersSingleEntityId(filters, eid)) {
      // Match, inserts a new reference
      EntityReference ref;
      auto a = it->second; // *IArchetype
      a->uncheckedGet(ref, __internal::unpack_y(eid));
      this->insert(eid, ref); // copy
    } else                    // Miss, remove
      this->erase(eid);
  };

  // Group filters by index pointers.
  std::unordered_set<Index *> indexPtrs;
  for (auto &f : filters)
    indexPtrs.insert(f->GetIndexPtr());
  // Register callbacks for each index.
  for (auto idx : indexPtrs)
    icbs.push_back({idx, idx->OnIndexValueUpdated(onIndexUpdated)}); // cppcheck-suppress useStlAlgorithm
}

// Executes given callback from internal cache container.
void ICacher::clearCallbacks() {
  for (auto [idx, callbackId] : icbs)
    idx->RemoveOnValueUpdatedCallback(callbackId);
  for (const auto callbackId : ecbs)
    world.RemoveCallback(callbackId);
}

// Executes given callback for each entity reference in cache.
void ICacher::ForEach(const Accessor &cb) {
  ForEachUntil([&cb](EntityReference &ref) {
    cb(ref);
    return false;
  });
}

} // namespace __internal

} // namespace tinyecs
