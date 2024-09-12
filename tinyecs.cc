// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/tinyecs
// Requirements: at least C++20.

#include <algorithm> // std::fill_n, std::sort
#include <cstdlib>	 // std::ldiv
#include <set>		 // std::set

#include "tinyecs.h"

namespace tinyecs
{

	namespace __internal
	{

		ComponentId			   IComponentBase::NextId = 0;
		static EntityReference NullEntityReference = {};

		// Use std::ldiv for faster divide and module operations, than / and % operators.
		//
		// From https://en.cppreference.com/w/cpp/numeric/math/div:
		//
		//   > On many platforms, a single CPU instruction obtains both the quotient and the remainder,
		//   > and this function may leverage that, although compilers are generally able to merge
		//   > nearby / and % where suitable.
		//
		// And the static_cast is safe here, a short entity id covers from 0 to 0x7ffff,
		// the `long` type guarantees at least 32 bit size.
		static inline std::pair<size_t, size_t> __div(EntityShortId e, size_t N)
		{
			// from cppreference: the returned std::ldiv_t might be either form of
			// { int quot; int rem; } or { int rem; int quot; }, the order of its members is undefined,
			// we have to pack them into a pair for further structured bindings.
			auto dv = std::ldiv(static_cast<long>(e), static_cast<long>(N));
			return { dv.quot, dv.rem };
		}

		/////////////////////////
		/// IArchetype
		/////////////////////////

		// Internal dev notes: Now a not so good solution is to temporarily store the ID of
		// the entity being created in the entire world before and after calling the initializer
		// (constructor of components by default) during the creation of an entity, so that the
		// field index can initialize the fieldproxy's value of this entity.
		// Progess demo:
		//   IArchetype -> setLastCreatedEntityId()
		//   IArchetype -> call constructors of entity's components.
		//                 -> FieldProxy's constructor called
		//                    -> get lastCreatedEntityId and insert to its index.
		//   IArchetype -> clearLastCreatedEntityId()
		void IWorld::SetLastCreatedEntityId(EntityId eid)
		{
			lastCreatedEntityId = eid;
			lastCreatedEntityIdSet = true;
		}

		// Returns true if given short entity id is inside the cemetery, aka already dead.
		bool Cemetery::Contains(EntityShortId e) const
		{
			if (e >= bound)
				return false;
			const auto [i, j] = __div(e, NumRowsPerBlock);
			return blocks[i]->test(j);
		}

		// Adds a short entity id into the cemetery.
		// If the underlying blocks are not enough, allocates until enough.
		void Cemetery::Add(EntityShortId e)
		{
			const auto [i, j] = __div(e, NumRowsPerBlock);

			// Allocates new blocks if not enough, at least i+1;
			Reserve(i + 1);
			blocks[i]->set(j);
			q.push_back(e);
		}

		// Pops a short entity id from management.
		// Must guarantee this cemetery is not empty in advance.
		EntityShortId Cemetery::Pop()
		{
			auto e = q.front();
			q.pop_front();
			const auto [i, j] = __div(e, NumRowsPerBlock);
			blocks[i]->reset(j);
			return e;
		}

		// Reserve for n blocks.
		void Cemetery::Reserve(size_t n)
		{
			while (n > blocks.size())
			{
				blocks.push_back(std::make_unique<std::bitset<NumRowsPerBlock>>());
				bound += NumRowsPerBlock;
			}
		}

		IArchetype::IArchetype(ArchetypeId id, IWorld* world, size_t numComponents, size_t cellSize,
			const Signature& signature)
			: id(id), world(world), numCols(numComponents + 1), cellSize(cellSize), rowSize(numCols * cellSize), blockSize(numRows * rowSize), signature(signature)
		{
			// 0xffff stands for invalid column.
			std::fill_n(cols, MaxNumComponents, 0xffff);

			// col starts from 1, skipping the seat of entity reference
			int col = 1;
			for (int i = 0; i < MaxNumComponents; i++)
				if (signature[i])
					cols[i] = col++;
		}

		// Returns the data address of entity e.
		// It doesn't care the entity's liveness, just return the data address that should be.
		unsigned char* IArchetype::AddressOf(EntityShortId e) const
		{
			// i is the index of the block in vector `blocks`, storing the given entity.
			// j is the index of the row in the block `i`, storing the given entity
			const auto [i, j] = __div(e, numRows);
			return blocks[i].get() + j * rowSize;
		}

		// Returns a reference to the target entity by giving a short entity id.
		// If the entity is not alive (dead or not born yet), returns NullEntityReference.
		EntityReference& IArchetype::Get(EntityShortId e)
		{
			if (!IsAlive(e))
				return NullEntityReference;
			return UncheckedGet(e);
		}

		// Returns the data address of a component according to given entity data.
		// Raises std::runtime_error on unknown component.
		unsigned char* IArchetype::GetComponentRawPtr(unsigned char* entityData, ComponentId cid) const
		{
			auto col = cols[cid];
			if (col == 0xffff)
				throw std::runtime_error("tinyecs: component of archetype " + std::to_string(id) + " not found");
			return entityData + col * cellSize;
		}

		// Allocates a seat for a new entity, including a short id and block row's memory address. Time: O(1)
		//  1. If the cemetery is not empty, reuse at first.
		//  2. Otherwise increases the internal short id cursor.
		//     What's more, allocates a new block on need.
		//  3. Constructs an entity reference at the row's head.
		std::pair<EntityShortId, unsigned char*> IArchetype::AllocateForNewEntity()
		{
			EntityShortId  e;
			unsigned char* data;

			if (cemetery.Size())
			{
				// Reuse dead entity id and space.
				e = cemetery.Pop();
				data = AddressOf(e);
				// Reset the space to all zeros.
				std::fill_n(data, rowSize, 0);
			}
			else
			{
				// Increase the entity id cursor.
				e = ecursor++;
				if (e >= numRows * blocks.size())
				{
					// Allocate a block if not enough
					blocks.push_back(std::make_unique<unsigned char[]>(blockSize));
					// Flush the full block space to all zeros.
					std::fill_n(blocks.back().get(), blockSize, 0);
				}
				data = AddressOf(e);
			}
			// Constructs an entity reference at the row's head.
			new (data) EntityReference(this, data, Pack(id, e));
			return { e, data };
		}

		// Makes an entity to be alive in the world.
		// 1. Add this entity to alives set.
		// 2. Constructs each component of this entity via initializer.
		// 3. Trigger callbacks listening for entity creations.
		void IArchetype::DoNewEntity(EntityShortId e, EntityReference& ref, Accessor& initializer)
		{
			// The entity is marked alive.
			alives.insert(e);

			// Call constructors of each component for this entity.
			// Before the constructor is called, set the entity id to the world in advance,
			// Because the index will query the lastCreatedEntityId in the world.
			// And the component's constructor may bind some indexes then.
			world->SetLastCreatedEntityId(ref.GetId());
			initializer(ref);
			world->ClearLastCreatedEntityId();
			world->AfterEntityCreated(id, e);
		}

		// Makes an entity to be born alive.
		// Does nothing if given entity is not in the toBorn map.
		void IArchetype::ApplyDelayedNewEntity(EntityShortId e)
		{
			auto it = toBorn.find(e);
			if (it == toBorn.end())
				return; // not find
			auto& initializer = it->second;
			auto  data = AddressOf(e);
			auto& ref = *ToRef(data);
			DoNewEntity(e, ref, initializer);
			toBorn.erase(it);
		}

		// Kill an entity by short id at once. O(logN).
		void IArchetype::Kill(EntityShortId e, Accessor* beforeKillPtr)
		{
			// Does nothing if given short id is invalid or already dead.
			if (e >= ecursor || cemetery.Contains(e))
				return;

			// If a kill is called on a to-born/to-kill entity, then it's going to die directly.
			auto data = AddressOf(e);

			// Call provided callback before all work.
			if (beforeKillPtr != nullptr)
				(*beforeKillPtr)(*ToRef(data));

			// Before removing hook.
			world->BeforeEntityRemoved(id, e);

			// Call destructor of each component.
			DestructComponents(data);

			// Call destructor of EntityReference.
			ToRef(data)->~EntityReference();

			// Remove from alives and add to cemetery.
			cemetery.Add(e);
			alives.erase(e);
		}

		// Mark an entity to be killed. O(1)
		// If the given entity is already dead, does nothing.
		// Parameter beforeKillPtr is a pointer to a function, which is to be execute before the killing is applied.
		void IArchetype::DelayedKill(EntityShortId e, Accessor* beforeKillPtr)
		{
			if (IsAlive(e))
			{
				const auto& callback = *beforeKillPtr;
				toKill.insert({ e, callback }); // copy
				world->AddDelayedKillEntity(id, e);
			}
		}

		// Kill a delayed to kill entity by short entity id.
		// Does nothing if given entity is not in the toKill map.
		void IArchetype::ApplyDelayedKill(EntityShortId e)
		{
			auto it = toKill.find(e);
			if (it == toKill.end())
				return;
			auto cb = it->second;
			Kill(e, &cb);
			toKill.erase(it);
		}

		EntityReference& IArchetype::NewEntity()
		{
			// Call the default constructor for each component if the initializer is not provided.
			return NewEntity([this](EntityReference& ref) { ConstructComponents(ToData(&ref)); });
		}

		EntityReference& IArchetype::NewEntity(Accessor& initializer)
		{
			// Allocates a seat for a new entity and call the initializer at once.
			auto [e, data] = AllocateForNewEntity();
			auto& ref = *ToRef(data);
			DoNewEntity(e, ref, initializer);
			return ref;
		}

		EntityId IArchetype::DelayedNewEntity()
		{
			// Call the default constructor for each component if the initializer is not provided.
			return DelayedNewEntity([this](EntityReference& ref) { ConstructComponents(ToData(&ref)); });
		}

		EntityId IArchetype::DelayedNewEntity(Accessor& initializer)
		{
			// A to-born entity is considered non-alive.
			// But we have to pre-allocate a seat for it, then its id and reference are available.
			// And the data can be set here in-place when applied.
			// In this design, we avoid data copy to support the "delayed new entity" feature.
			auto [e, data] = AllocateForNewEntity();

			// here copy initializer function to store
			toBorn.insert({ e, initializer });
			world->AddDelayedNewEntity(id, e);
			return Pack(id, e);
		}

		void IArchetype::ForEachUntilForward(const AccessorUntil& cb)
		{
			// Scan the set alives from begin to end, in order of address layout.
			// Use an ordered set of alive short entity ids instead of scan from 0 to cursor directly
			// for faster speed (less miss).
			// It's undefined behavor to kill or add entities during the foreach, in the callback.
			for (auto e : alives)
			{
				if (!cemetery.Contains(e) && !toBorn.contains(e))
					if (cb(UncheckedGet(e)))
						break;
			}
		}

		void IArchetype::ForEachUntilBackward(const AccessorUntil& cb)
		{
			for (auto it = alives.rbegin(); it != alives.rend(); ++it)
			{
				auto e = *it;
				if (!cemetery.Contains(e) && !toBorn.contains(e))
					if (cb(UncheckedGet(e)))
						break;
			}
		}

		void IArchetype::ForEach(const Accessor& cb, const bool reversed)
		{
			ForEachUntil(
				[&cb](EntityReference& ref) {
					cb(ref);
					return false;
				},
				reversed);
		}

		void IArchetype::ForEachUntil(const AccessorUntil& cb, const bool reversed)
		{
			if (!reversed)
			{
				ForEachUntilForward(cb);
				return;
			}
			ForEachUntilBackward(cb);
		}

		void IArchetype::Reserve(size_t numEntities)
		{
			auto [i, j] = __div(numEntities, numRows);
			auto numBlocks = i + (j != 0);

			while (numBlocks > blocks.size())
			{
				// Allocates new blocks.
				blocks.push_back(std::make_unique<unsigned char[]>(blockSize));
				std::fill_n(blocks.back().get(), blockSize, 0);
			}

			// Reserve for cemetery.
			cemetery.Reserve(numBlocks);

			// Reserve for unordered_maps.
			toBorn.reserve(numEntities);
			toKill.reserve(numEntities);
		}

		//////////////////////////
		/// Matcher
		//////////////////////////

		void Matcher::PutArchetypeId(const Signature& signature, ArchetypeId aid)
		{
			all[aid] = 1;
			for (int i = 0; i < MaxNumComponents; i++)
				if (signature[i])
					b[i].set(aid);
		}

		const AIdsPtr Matcher::MatchAndStore(MatchRelation relation, const Signature& signature)
		{
			auto ans = Match(relation, signature);
			auto ptr = std::make_shared<AIds>(std::move(ans));
			store.push_back(ptr);
			return ptr;
		}

		AIds Matcher::Match(MatchRelation relation, const Signature& signature) const
		{
			ArchetypeIdBitset ans;
			switch (relation)
			{
				case MatchRelation::ALL:
					ans = MatchAll(signature);
					break;
				case MatchRelation::ANY:
					// For Any<>, means matching all.
					if (signature.none())
						ans = all;
					else
						ans = MatchAny(signature);
					break;
				case MatchRelation::NONE:
					ans = MatchNone(signature);
					break;
			}

			// Make an unordered set of matched archetype ids.
			AIds results;
			for (int i = 0; i < MaxNumArchetypesPerWorld; i++)
				if (ans[i])
					results.insert(i);
			return results;
		}

		Matcher::ArchetypeIdBitset Matcher::MatchAll(const Signature& signature) const
		{
			// all & b[c1] & b[c2] & ...
			ArchetypeIdBitset ans = all;
			for (int i = 0; i < MaxNumComponents; i++)
				if (signature[i])
					ans &= b[i];
			return ans;
		}

		Matcher::ArchetypeIdBitset Matcher::MatchAny(const Signature& signature) const
		{
			// b[c1] | b[c2] | ...
			ArchetypeIdBitset ans;
			for (int i = 0; i < MaxNumComponents; i++)
				if (signature[i])
					ans |= b[i];
			return ans;
		}

		Matcher::ArchetypeIdBitset Matcher::MatchNone(const Signature& signature) const
		{
			return all & ~MatchAny(signature);
		}

	} // namespace __internal

	//////////////////////////
	/// World
	//////////////////////////

	bool World::IsAlive(EntityId eid) const
	{
		auto aid = __internal::UnpackX(eid);
		if (aid >= archetypes.size())
			return false;
		return archetypes[aid]->IsAlive(__internal::UnpackY(eid));
	}

	void World::Kill(EntityId eid)
	{
		auto aid = __internal::UnpackX(eid);
		if (aid >= archetypes.size())
			return;
		archetypes[aid]->Kill(__internal::UnpackY(eid), nullptr);
	}

	void World::DelayedKill(EntityId eid)
	{
		auto aid = __internal::UnpackX(eid);
		if (aid >= archetypes.size())
			return;
		archetypes[aid]->DelayedKill(__internal::UnpackY(eid), nullptr);
	}

	void World::DelayedKill(EntityId eid, Accessor& beforeKilled)
	{
		auto aid = __internal::UnpackX(eid);
		if (aid >= archetypes.size())
			return;
		archetypes[aid]->DelayedKill(__internal::UnpackY(eid), &beforeKilled);
	}

	EntityReference& World::Get(EntityId eid) const
	{
		auto aid = __internal::UnpackX(eid);
		if (aid >= archetypes.size())
			return __internal::NullEntityReference;
		auto& a = archetypes[aid];
		return a->Get(__internal::UnpackY(eid));
	}

	EntityReference& World::UncheckedGet(EntityId eid) const
	{
		auto  aid = __internal::UnpackX(eid);
		auto& a = archetypes[aid];
		return a->UncheckedGet(__internal::UnpackY(eid));
	}

	void World::RemoveCallback(uint32_t id)
	{
		auto it = callbacks.find(id);
		if (it == callbacks.end())
			return;
		auto& cb = it->second;
		for (const auto aid : *(cb->aids))
			std::erase_if(callbackTable[cb->flag][aid], [&cb](auto x) { return x == cb.get(); });
		callbacks.erase(it);
	}

	void World::ApplyDelayedKills()
	{
		while (!toKill.empty())
		{
			auto eid = toKill.front();
			toKill.pop_front();
			auto aid = __internal::UnpackX(eid);
			if (aid < archetypes.size())
			{
				archetypes[aid]->ApplyDelayedKill(__internal::UnpackY(eid));
			}
		}
	}

	void World::ApplyDelayedNewEntities()
	{
		while (!toBorn.empty())
		{
			auto eid = toBorn.front();
			toBorn.pop_front();
			auto aid = __internal::UnpackX(eid);
			if (aid < archetypes.size())
			{
				archetypes[aid]->ApplyDelayedNewEntity(__internal::UnpackY(eid));
			}
		}
	}

	// Push a callback function into management to subscribe create and kill enntities events
	// inside any of given archetypes.
	uint32_t World::PushCallback(int flag, const __internal::AIdsPtr aids, const Callback::Func& func)
	{
		uint32_t id = nextCallbackId++; // TODO: handle overflow?
		auto	 cb = std::make_unique<Callback>(id, flag, func, aids);
		for (const auto aid : *aids)
			callbackTable[flag][aid].push_back(cb.get());
		callbacks[id] = std::move(cb);
		return id;
	}

	void World::TriggerCallbacks(ArchetypeId aid, EntityShortId e, int flag)
	{
		const auto& callbacks = callbackTable[flag][aid];
		auto		a = archetypes[aid].get();
		for (const auto* cb : callbacks)
		{
			cb->func(a->UncheckedGet(e));
		}
	}

	//////////////////////////
	/// Filter
	//////////////////////////

	namespace __internal
	{

		uint32_t IFieldIndexRoot::OnIndexValueUpdated(const CallbackOnIndexValueUpdated& cb)
		{
			uint32_t id = nextCallbackId++;
			callbacks[id] = cb;
			return id;
		}

		void IFieldIndexRoot::OnUpdate(EntityId eid)
		{
			for (auto& [_, cb] : callbacks)
				cb(this, eid);
		}

		//////////////////////////
		/// Query
		//////////////////////////

		using EntityIdSet = std::unordered_set<EntityId>; // unordered set of entity ids

		// ~~~~~~~~~~ IQuery Filters ~~~~~~~~~~~~~~~~~~~

		// Apply filters starting from a position to given set of entity ids ans.
		// This will remove not-satisfied entity ids from the set inplace.
		void applyFiltersFrom(const Filters& filters, EntityIdSet& ans, size_t start = 0)
		{
			// cb fills {ans & filter} into set tmp.
			EntityIdSet tmp;

			CallbackFilter cb = [&ans, &tmp](EntityId eid) {
				if (ans.contains(eid))
					tmp.insert(eid);
				return tmp.size() == ans.size(); // stop if all satisfied.
			};

			for (size_t i = start; i < filters.size() && ans.size(); i++)
			{
				// stop once ans is empty
				filters[i]->Execute(cb);
				ans.swap(tmp), tmp.clear();
			}
		}

		// Test given filters on a single entity, returns true if the entity satisfy all filters.
		bool testFiltersSingleEntityId(const Filters& filtrers, EntityId eid)
		{
			EntityIdSet ans{ eid };
			applyFiltersFrom(filtrers, ans, 0);
			return !ans.empty();
		}

		// Apply all given filters to given entity id set.
		// It firstly collects all entity ids matching the first filter via initialCollector.
		// And then run remaining filters to shrink the initial set inplace.
		void applyFilters(const Filters& filters, EntityIdSet& ans, CallbackFilter& initialCollector)
		{
			filters[0]->Execute(initialCollector);
			applyFiltersFrom(filters, ans, 1);
		}

		inline void applyFilters(const Filters& filters, EntityIdSet& ans, CallbackFilter&& initialCollector)
		{
			applyFilters(filters, ans, initialCollector);
		}

		// ~~~~~~~~~~ IQuery Internals ~~~~~~~~~~~~~~~~~~~

		// Match archetypes.
		IQuery& IQuery::PreMatch()
		{
			if (ready)
				return *this;
			if (world.archetypes.empty())
				throw std::runtime_error("tinyecs: query PreMatch must be called after archetypes are all created");

			aids = world.matcher->MatchAndStore(relation, signature);
			const auto& aidSet = *aids;

			// redundancy pointers of matched archetypes
			for (const auto aid : aidSet)
				archetypes[aid] = world.archetypes[aid].get();

			// redundancy a vector of sorted archetype ids
			orderedAids = std::vector<ArchetypeId>(aidSet.begin(), aidSet.end());
			std::sort(orderedAids.begin(), orderedAids.end());
			ready = true;
			return *this;
		}

		// Executes given callback with filters.
		// Internal notes:
		// We use an unordered_set to apply filters, instead of a ordered set.
		// And then copy to sorted sets groupping by archetype ids.
		// Reason: expecting set ans is smaller after filters applied,
		// this reduces item count to sort.
		void IQuery::ExecuteWithFilters(const AccessorUntil& cb, bool reversed)
		{
			// Filter matched entities from indexes.
			EntityIdSet ans;

			applyFilters(filters, ans, [&ans, this](EntityId eid) {
				// Must belong to interested archetypes.
				if (archetypes.contains(UnpackX(eid)))
					ans.insert(eid);
				return false;
			});

			// Sort entity ids to make scanning in an archetype are as continuous
			// in memory as possible.
			// TODO: any optimization ideas here to avoid sorting?
			std::set<EntityId> st(ans.begin(), ans.end());
			ExecuteWithinFilteredSet(cb, st, reversed);
		}

		void IQuery::ExecuteWithinFilteredSet(const AccessorUntil& cb, const std::set<EntityId>& st, bool reversed)
		{
			if (!reversed)
			{
				ExecuteWithinFilteredSetForward(cb, st);
				return;
			}
			ExecuteWithinFilteredSetBackward(cb, st);
		}

		void IQuery::ExecuteWithinFilteredSetForward(const AccessorUntil& cb, const std::set<EntityId>& st)
		{
			for (auto eid : st)
			{
				// Run callback function for each entity.
				// Note: No need to check whether an entity belongs to this query's archetypes.
				// Because the function initial collector of the first filter guarantees it.
				auto aid = __internal::UnpackX(eid);

				// Use get instead of uncheckedGet to ensure the entity is still alive.
				// Given callback might kill some entity.
				// TODO: shall we use uncheckedGet here? if user promise there's no entity killings
				// inside the callback.
				auto& ref = archetypes[aid]->Get(__internal::UnpackY(eid));
				if (cb(ref))
					break;
			}
		}

		void IQuery::ExecuteWithinFilteredSetBackward(const AccessorUntil& cb, const std::set<EntityId>& st)
		{
			for (auto it = st.rbegin(); it != st.rend(); ++it)
			{
				auto  eid = *it;
				auto  aid = __internal::UnpackX(eid);
				auto& ref = archetypes[aid]->Get(__internal::UnpackY(eid));
				if (cb(ref))
					break;
			}
		}

		// Executes given callback directly on each interested archetypes.
		void IQuery::ExecuteForAll(const AccessorUntil& cb, bool reversed)
		{
			if (!reversed)
			{
				ExecuteForAllForward(cb);
				return;
			}
			ExecuteForAllBackward(cb);
		}

		void IQuery::ExecuteForAllForward(const AccessorUntil& cb)
		{
			for (auto aid : orderedAids)
				archetypes[aid]->ForEachUntilForward(cb);
		}

		void IQuery::ExecuteForAllBackward(const AccessorUntil& cb)
		{
			for (auto it = orderedAids.rbegin(); it != orderedAids.rend(); ++it)
			{
				auto aid = *it;
				archetypes[aid]->ForEachUntilBackward(cb);
			}
		}

		// ~~~~~~~~~~ IQuery API ~~~~~~~~~~~~~~~~~~~

		// Push a single filter into query's management (copy).
		IQuery& IQuery::Where(const Filter& f)
		{
			filters.push_back(f);
			return *this;
		}

		// Push a single filter into query's management (move).
		IQuery& IQuery::Where(Filter&& f)
		{
			filters.push_back(std::move(f));
			return *this;
		}

		// Push a list of filters into query's management (copy).
		IQuery& IQuery::Where(const Filters& fl)
		{
			for (const auto& f : fl)
				filters.push_back(f);
			return *this;
		}

		// Push a list of filters into query's management (move).
		IQuery& IQuery::Where(Filters&& fl)
		{
			for (auto&& f : fl)
				filters.push_back(std::move(f));
			return *this;
		}

		IQuery& IQuery::ClearFilters()
		{
			filters.clear();
			return *this;
		}

		// Iterates each matched entities.
		void IQuery::ForEach(const Accessor& cb, bool reversed)
		{
			ForEachUntil(
				[&cb](EntityReference& ref) {
					cb(ref);
					return false;
				},
				reversed);
		}

		// ForEachUntil is the ForEach that will stop once cb returns true.
		void IQuery::ForEachUntil(const AccessorUntil& cb, bool reversed)
		{
			if (!ready)
				throw std::runtime_error("tinyecs: Query PreMatch not called");
			if (archetypes.empty())
				return; // early quit.
			if (filters.empty())
				return ExecuteForAll(cb, reversed);
			ExecuteWithFilters(cb, reversed);
		}

		void IQuery::Collect(std::vector<EntityReference>& vec, bool reversed)
		{
			ForEachUntil(
				[&vec](EntityReference& ref) {
					vec.push_back(ref); // copy
					return false;
				},
				reversed);
		}

		void IQuery::CollectUntil(std::vector<EntityReference>& vec, AccessorUntil& tester, bool reversed)
		{
			ForEachUntil(
				[&vec, &tester](EntityReference& ref) {
					// Stops early once the tester returns a true.
					if (tester(ref))
						return true;
					vec.push_back(ref); // copy
					return false;
				},
				reversed);
		}

		//////////////////////////
		/// Cacher
		//////////////////////////

		// Executes the query at once and cache them into cache container.
		// Then setup callbacks to watch changes.
		void ICacher::Setup(__internal::IQuery& q)
		{
			if (aids->empty())
				return; // early quit.

			// Cache entities right now (copy)
			// In the order of the query's order (backward or forward).
			q.ForEach([this](const EntityReference& ref) { this->Insert(ref.GetId(), ref); });

			// Setup callbacks.
			SetupCallbacksWatchingEntities();
			SetupCallbacksWatchingIndexes();
		}

		// Setup callbacks to watch entities in interested archetypes.
		// OnEntityCreated: add into the cache if match.
		// OnEntityRemoved: remove from cache.
		void ICacher::SetupCallbacksWatchingEntities()
		{
			CallbackAfterEntityCreated onEntityCreated = [this](const EntityReference& ref) {
				// Checks if this entity satisfies all filters if presents.
				if (filters.size() && !testFiltersSingleEntityId(filters, ref.GetId()))
					return;

				// Note: there's no need to recheck whether the entity is inside the query's interested archetypes.
				// Because we subscribe callbacks by corresponding archetype ids.
				this->Insert(ref.GetId(), ref);
			};
			CallbackBeforeEntityRemoved onEntityRemoved = [this](const EntityReference& ref) {
				// Remove anyway, it's going to die, no need to check filters at all.
				this->Erase(ref.GetId());
			};
			ecbs.push_back(world.PushCallback(0, aids, onEntityCreated));
			ecbs.push_back(world.PushCallback(1, aids, onEntityRemoved));
		}

		// Setup callbacks to watch index value updates.
		// On index updated: test all filters for corresponding entity.
		// No need to watch index value inserts and removes, because we
		// already watch those via entitiy life cycle callbacks.
		void ICacher::SetupCallbacksWatchingIndexes()
		{
			if (filters.empty())
				return;
			using Callback = __internal::CallbackOnIndexValueUpdated;
			using Index = __internal::IFieldIndexRoot;
			Callback onIndexUpdated = [this](const Index* idx, EntityId eid) {
				// Must be alive in any of our archetypes.
				ArchetypeId aid = __internal::UnpackX(eid);
				auto		it = archetypes.find(aid);
				if (it == archetypes.end())
					return;

				// Test this single entity with all filters.
				// Note: we can't test the filters only associated with the updated index.
				// Because there may be some other filters dismatches this entity.
				if (testFiltersSingleEntityId(filters, eid))
				{
					// Match, inserts a new reference
					auto a = it->second; // *IArchetype

					// Copy into cache container.
					this->Insert(eid, a->UncheckedGet(__internal::UnpackY(eid)));
				}
				else // Miss, remove
					this->Erase(eid);
			};

			// Group filters by index pointers.
			std::unordered_set<Index*> indexPtrs;
			for (auto& f : filters)
				indexPtrs.insert(f->GetIndexPtr());
			// Register callbacks for each index.
			for (auto idx : indexPtrs)
				icbs.push_back({ idx, idx->OnIndexValueUpdated(onIndexUpdated) }); // cppcheck-suppress useStlAlgorithm
		}

		// Executes given callback from internal cache container.
		void ICacher::ClearCallbacks()
		{
			for (auto [idx, callbackId] : icbs)
				idx->RemoveOnValueUpdatedCallback(callbackId);
			for (const auto callbackId : ecbs)
				world.RemoveCallback(callbackId);
		}

		// Executes given callback for each entity reference in cache.
		void ICacher::ForEach(const Accessor& cb, bool reversed)
		{
			ForEachUntil(
				[&cb](EntityReference& ref) {
					cb(ref);
					return false;
				},
				reversed);
		}

		void ICacher::Collect(std::vector<EntityReference>& vec, bool reversed)
		{
			ForEachUntil(
				[&vec](EntityReference& ref) {
					vec.push_back(ref); // copy
					return false;
				},
				reversed);
		}

		void ICacher::CollectUntil(std::vector<EntityReference>& vec, AccessorUntil& tester, bool reversed)
		{
			ForEachUntil(
				[&vec, &tester](EntityReference& ref) {
					// stop early once tester given true.
					if (tester(ref))
						return true;
					vec.push_back(ref); // copy
					return false;
				},
				reversed);
		}

	} // namespace __internal

} // namespace tinyecs
