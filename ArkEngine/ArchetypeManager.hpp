#pragma once

#include "Core.hpp"
#include "Util.hpp"
#include "gsl.hpp"
#include "Script.hpp"

#include <bitset>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <type_traits>
#include <typeindex>
#include <map>
#include <tuple>

// future entity archetype manager to write
// components of a single entity are stored in the same buffer, as oposed to storing them in pools

namespace ark {

	class ComponentManager;
	class Entity;

	class Archetype {

	public:
		Archetype() = default;
		~Archetype() = default;


		template<typename T>
		void addComponentType()
		{
			static_assert(std::is_default_constructible_v<T>);
			auto hasType = Util::find(types, typeid(T));
			if (!hasType) {
				types.push_back(typeid(T));
			}
		}

		std::vector<std::string> getComponentNames()
		{
			std::vector<std::string> names;
			for (const auto& type : types)
				names.push_back(type.name());
			return names;
		}

	private:
		std::vector<std::type_info> types{};
		friend class ComponentManager;
	};


	// component handle
	template <typename T>
	class StableReference {

	public:
		explicit StableReference() = default;

		StableReference(const StableReference<T>& h)
		{
			copy(h);
			inc();
		}

		StableReference& operator=(const StableReference<T>& h)
		{
			copy(h);
			inc();
			return *this;
		}

		StableReference(StableReference&& h)
		{
			copy(h);
			h.component = nullptr;
			h.counter = nullptr;
		}

		StableReference& operator=(StableReference&& h)
		{
			copy(h);
			h.component = nullptr;
			h.counter = nullptr;
			return *this;
		}

		T* operator->() { return (*component); }
		T& get() { return *(*component); }
		T& operator*() { return get(); }
		bool isValid() { return (counter && component); }

		~StableReference()
		{
			dec();
		}

	private:
		template <typename T>
		void copy(T&& h)
		{
			this->component = h.component;
			this->counter = h.counter;
		}

		void inc()
		{
			if (counter)
				(*counter)++;
		}
		void dec()
		{
			if (counter)
				(*counter)--;
		}

		T** component = nullptr;
		int* counter = nullptr;

		friend class ComponentManager;
	};



	class ComponentManager {

	public:
		static inline constexpr std::size_t MaxComponentTypes = 64;
		using ArchetypeMask = std::bitset<MaxComponentTypes>;

	private:
		using byte = std::byte;

		struct ComponentMetadata;
		struct ArchetypeStorage;
		struct FreeSlot;
		struct HandleData;
		struct Chunk;

	public:
		// taken as parameter
		struct EntityData {
			ArchetypeMask mask;
			int16_t chunkNum; // starts at 0;
			int16_t entityNum; // also starts at 0; inside chunk
		};

		struct EntityDataRef {
			ArchetypeMask& mask;
			int16_t& chunkNum;
			int16_t& entityNum;
		};

		template <typename T>
		bool hasComponentType()
		{
			return Util::find_if(metadataPool, [](auto& m) { return m.type == typeid(T); });
		}


		int getComponentId(std::type_index type)
		{
			auto pos = std::find_if(std::begin(metadataPool), std::end(metadataPool), [type](auto& m) {return m.type == type; });
			if (pos == std::end(metadataPool)) {
				std::cout << "component manager dosent have component type: " << type.name();
				return ArkInvalidIndex;
			}
			return pos - std::begin(metadataPool);
		}


		template <typename T>
		int getComponentId()
		{
			return getComponentId(typeid(T));
		}


		template <typename T>
		void addComponentType()
		{
			if (hasComponentType<T>())
				return;
			if (metadataPool.size() == MaxComponentTypes) {
				std::cerr << "nu mai e loc de tipuri de componente, max: " << MaxComponentTypes << std::endl << std::endl;
				return;
			}
			auto& metadata = metadataPool.emplace_back();
			metadata.constructMetadata<T>();
		}


		template <typename... Ts>
		Archetype createArchetype()
		{
			Archetype a;
			((a.addComponentType<Ts>()), ...);
			(addComponentType<Ts>(), ...);
			return a;
		}


		EntityData createComponents(Archetype archetype)
		{
			// construct mask
			EntityData entityData;
			for (const auto& type : archetype.types)
				entityData.mask.set(getComponentId(type));

			auto storage = getStorageAndConstructIfNotExistent(entityData.mask);
			auto [chunkNum, entityNum] = getAvailableSlot(storage, entityData.mask);
			entityData.chunkNum = chunkNum;
			entityData.entityNum = entityNum;

			auto& chunk = storage.chunks.at(chunkNum);
			auto buffer = getBuffer(entityNum, chunk, storage);
			byte* component = buffer.data();

			for (auto metadata : storage.metadata) {
				if (metadata->isDefaultConstructible)
					metadata->default_construct(component);
				component += metadata->size;
			}
			return entityData;
		}


		template <typename T, typename...Args>
		T& addComponent(EntityDataRef entityData, int entityId, Args&& ... args)
		{
			// muta check-ul asta in EntityManager
			if (entityData.mask.test(getComponentId<T>()))
				return getComponent<T>(entityData);

			if (entityData.chunkNum == ArkInvalidIndex) {
				// first component added
				entityData.mask.set(getComponentId<T>());
				auto& storage = getStorageAndConstructIfNotExistent(entityData.mask);
				auto [chunkNum, entityNum] = getAvailableSlot(storage, entityData.mask);
				entityData.chunkNum = chunkNum;
				entityData.entityNum = entityNum;

				auto& chunk = storage.chunks.at(chunkNum);
				//byte* slot = chunk.buffer.data() + (entityNum * storage.entitySize);
				auto buffer = getBuffer(entityData.entityNum, chunk, storage);
				chunk.numberOfEntities += 1;
				Util::construct_in_place<T>(buffer.data(), std::forward<Args>(args...));
				return reinterpret_cast<T&>(*buffer.data());

			} else {
				// move to new archetype and add new component
				auto [oldBuffer, oldStorage] = getBuffer(entityData);
				auto& oldChunk = oldStorage.chunks.at(entityData.chunkNum);
				oldChunk.numberOfEntities -= 1;
				auto oldMask = entityData.mask;
				entityData.mask.set(getComponentId<T>());

				auto& storage = getStorageAndConstructIfNotExistent(entityData.mask);

				auto [chunkNum, entityNum] = getAvailableSlot(storage, entityData.mask);

				markFreeSlot({oldMask, entityData.chunkNum, entityData.entityNum});

				entityData.chunkNum = chunkNum;
				entityData.entityNum = entityNum;

				auto& chunk = storage.chunks.at(chunkNum);
				//byte* pEntityBuffer = chunk.buffer.data() + entityNum;
				auto buffer = getBuffer(entityData, chunk, storage);
				chunk.numberOfEntities += 1;

				int newComponentPos = Util::get_index_if(storage.metadata, [](auto& md) { return md->type == typeid(T); });
				int newComponentSize = storage.metadata.at(newComponentPos)->size;

				int leftComponentsSize = 0;
				for (int i = 0; i < newComponentPos; i++)
					leftComponentsSize += storage.metadata.at(i)->size;

				int rightComponentsSize = 0;
				for (int i = newComponentPos + 1; i < storage.metadata.size(); i++)
					rightComponentsSize += storage.metadata.at(i)->size;
				byte* pRightComponents = buffer.data() + leftComponentsSize + newComponentSize;
				//byte* pRightComponents = pEntityBuffer + storage.entitySize - rightComponentsSize;

				// copy components to the left of the new component
				std::memcpy(buffer.data(), oldBuffer.data(), leftComponentsSize);

				// construct new component
				Util::construct_in_place<T>(buffer.data() + leftComponentsSize, std::forward<Args>(args)...);

				// copy components to the right of the new component
				std::memcpy(pRightComponents, oldBuffer.data() + leftComponentsSize, rightComponentsSize);

				updateStableReferences(entityData, entityId);

				return *reinterpret_cast<T*>(buffer.data() + leftComponentsSize);
			}
		}


		template <typename T>
		T& getComponent(const EntityData& entityData)
		{
			auto [buffer, storage] = this->getBuffer(entityData);

			int pos = 0;
			for (auto metadata : storage.metadata)
				if (metadata->type != typeid(T))
					pos += metadata->size;
				else
					break;
			return reinterpret_cast<T&>(buffer.data() + pos);
		}


		template <typename T1, typename T2>
		std::tuple<T1&, T2&> getComponent(const EntityData& entityData)
		{
			auto [buffer, storage] = this->getBuffer(entityData);

			int pos1 = 0;
			int pos2 = 0;
			bool found1 = false;
			bool found2 = false;

			for (auto metadata : storage.metadata) {
				if (!found1 && metadata->type != typeid(T1)) {
					pos1 += metadata->size;
				} else {
					found1 = true;
				}
				if (!found2 && metadata->type != typeid(T2)) {
					pos2 += metadata->size;
				} else {
					found2 = true;
				}
			}

			T1& comp1 = reinterpret_cast<T1&>(buffer.data() + pos1);
			T2& comp2 = reinterpret_cast<T2&>(buffer.data() + pos2);
			return std::make_tuple<T1&, T2&>(comp1, comp2);
		}


		template <typename... Ts>
		std::tuple<Ts& ...> getComponents(const EntityData& entityData)
		{
			auto [buffer, storage] = this->getBuffer(entityData);

			std::tuple<internal_pair_type<Ts>...> positions;

			auto processPos = [](auto& metadata, auto& p) {
				if (!p.found && metadata->type != typeid(decltype(p)::CompT))
					p.pos += metadata->size;
				else
					p.found = true;
			};

			for (auto metadata : storage.metadata) {
				std::apply([metadata](auto& ... pos) {
					(processPos(metadata, pos), ...);
				}, positions);
			}

			auto getRef = [&buffer](auto& p) -> typename decltype(p)::CompT & {
				return reinterpret_cast<typename decltype(p)::CompT&>(buffer.data() + p.pos);
			};

			return std::apply([](auto& ... pos) {
				return std::make_tuple<Ts & ...>((getRef(pos), ...));
			}, positions);
		}


		template <typename T>
		StableReference<T> getStableReference(const EntityData& entityData, int entityId)
		{
			if (!hasComponentType<T>())
				return; // fa urat, gen abort

			auto& handles = handlePools[entityId];

			if (auto found = Util::find_if(handles, [](auto& hData) { return hData.type == typeid(T); }); found) {
				HandleData& hData = found.value();
				hData.counter++;

				StableReference<T> ref;
				ref.component = &reinterpret_cast<T*>(hData.pComponent);
				ref.counter = &hData.counter;
				return ref;

			} else {
				HandleData& hData = handles.emplace_back();
				hData.type = typeid(T);
				hData.pComponent = &getComponent<T>(entityData);
				hData.counter = 1;

				StableReference<T> ref;
				ref.counter = &hData.counter;
				ref.component = &reinterpret_cast<T*>(hData.pComponent);
				return ref;
			}
		}


		EntityData cloneEntity(const EntityData& entityData)
		{
			auto [clonedBuffer, storage] = getBuffer(entityData);
			auto [chunkNum, entityNum] = getAvailableSlot(storage, entityData.mask);

			EntityData cloneEntityData;
			cloneEntityData.mask = entityData.mask;
			cloneEntityData.chunkNum = chunkNum;
			cloneEntityData.entityNum = entityNum;
			auto [newBuffer, _] = getBuffer(cloneEntityData);

			byte* newBufferPos = newBuffer.data();
			byte* clonedBufferPos = clonedBuffer.data();

			for (auto metadata : storage.metadata) {
				// maybe add copy assign?
				if (metadata->isCopyable) {
					metadata->copy_construct(newBufferPos, clonedBufferPos);
				} else if (metadata->isDefaultConstructible) {
					metadata->default_construct(newBufferPos);
				}
				newBufferPos += metadata->size;
				clonedBufferPos += metadata->size;
			}
			return cloneEntityData;
		}


		// TODO: remove component
		// apropo, de ce ar vrea cineva sa scoata o componenta?
		template <typename T>
		void removeComponent(EntityDataRef entityData, int entityId)
		{
			updateStableReferences(entityData, entityId);
		}


		void destroyEntity(const EntityData& entityData, int entityId)
		{
			auto [buffer, storage] = getBuffer(entityData);
			byte* component = buffer.data();
			for (auto metadata : storage.metadata) {
				metadata->destruct(component);
				component += metadata->size;
			}
			markFreeSlot(entityData);
			handlePools.erase(entityId);
		}


		// delete unused stable references
		void cleanHandles()
		{
			for (auto& [entityId, handles] : handlePools) {
				std::vector<int> indexes;
				int index = 0;
				for (auto& handle : handles) {
					if (handle.counter <= 0)
						indexes.push_back(index);
					index += 1;
				}
				for (int index : indexes) {
					Util::erase_at(handles, index);
				}
			}
		}

		// pair.first = previousChunkNum, pair.second = newChunkNum
		auto cleanEmptyArchetypesAndEmptyChunks() -> std::unordered_map<ArchetypeMask, std::vector<std::pair<int16_t, int16_t>>>
		{
			std::unordered_map<ArchetypeMask, std::vector<std::pair<int16_t, int16_t>>> entityData;
			std::vector<ArchetypeMask> emptyStorageKeys;

			for (auto& [mask, storage] : storagePools) {
				std::vector<std::pair<int16_t, int16_t>> emptyChunksIndexes;

				int chunkIndex = 0;
				for (auto& chunk : storage.chunks) {
					if (chunk.numberOfEntities <= 0) {
						std::pair<int16_t, int16_t> pair;
						pair.first = chunkIndex;
						// scadem nr de chunk-uri care au fost sterse
						pair.second = chunkIndex - emptyChunksIndexes.size();
						emptyChunksIndexes.push_back(pair);
					}
					chunkIndex += 1;
				}
				for (auto [prevChunk, newChunk] : emptyChunksIndexes)
					Util::erase_at(storage.chunks, prevChunk);

				if (storage.chunks.empty())
					emptyStorageKeys.push_back(mask);
				else if (!emptyChunksIndexes.empty())
					//entityData[mask].emplace_back(std::move(emptyChunksIndexes));
					entityData[mask] = std::move(emptyChunksIndexes);
			}

			for (auto mask : emptyStorageKeys)
				storagePools.erase(mask);

			return entityData;
		}

	private:

		void markFreeSlot(EntityData entityData)
		{
			FreeSlot slot;
			slot.chunkNum = entityData.chunkNum;
			slot.entityNum = entityData.entityNum;
			freeSlotPools[entityData.mask].push_back(slot);
		}

		// allocate new chunk if available memory is not found
		// return [chunkNum, entityNum]
		std::pair<int16_t, int16_t> getAvailableSlot(ArchetypeStorage& storage, const ArchetypeMask& mask)
		{
			// search for a free slot
			if (auto found = freeSlotPools.find(mask); found != freeSlotPools.end()) {
				auto& slots = found->second;
				if (!slots.empty()) {
					FreeSlot slot = slots.back();
					slots.pop_back();
					return {slot.chunkNum, slot.entityNum};
				}
			}

			auto* lastChunk = &storage.chunks.back();
			// use the last chunk or allocate a new chunk if we need to
			if (lastChunk->numberOfEntities == storage.maxNumberOfEntitiesPerChunk) {
				lastChunk = &storage.chunks.emplace_back();
			}

			int16_t chunkNum = storage.chunks.size() - 1;
			int16_t entityNum = lastChunk->numberOfEntities; // inca nu e incrementat deci nu e -1

			return {chunkNum, entityNum};
		}

		void updateStableReferences(const EntityData& entityData, int entityId)
		{
			auto found = handlePools.find(entityId);
			if (found == handlePools.end())
				return;

			auto& handles = found->second;
			auto [buffer, storage] = getBuffer(entityData);

			byte* component = buffer.data();
			for (auto metadata : storage.metadata) {
				for (auto& handle : handles) {
					if (handle.type == metadata->type) {
						handle.pComponent = component;
					}
				}
				component += metadata->size;
			}
		}

		auto getChunk(const EntityData& entityData) -> std::pair<Chunk&, ArchetypeStorage&>
		{
			auto& storage = storagePools.find(entityData.mask)->second;;
			auto& chunk = storage.chunks.at(entityData.chunkNum);
			return {chunk, storage};
		}

		gsl::span<byte> getBuffer(int16_t entityNum, Chunk& chunk, ArchetypeStorage& storage)
		{
			int beginPos = storage.entitySize * entityNum;
			int endPos = storage.entitySize * (entityNum + 1);
			if (endPos >= chunk.buffer.size())
				std::cout << "am depasit marimea chunk-ului din component manager, ai futut-o rau!" << "\n\n";
			gsl::span<byte> buffer(chunk.buffer.data() + beginPos, endPos);
			return buffer;
		}

		auto getBuffer(const EntityData& entityData) -> std::pair<gsl::span<byte>, ArchetypeStorage&>
		{
			auto [chunk, storage] = getChunk(entityData);
			auto buffer = getBuffer(entityData.entityNum, chunk, storage);
			return {buffer, storage};
		}

		ArchetypeStorage& getStorageAndConstructIfNotExistent(ArchetypeMask mask)
		{
			auto storage = storagePools.find(mask);
			if (storage != storagePools.end()) { // return it
				return storage->second;
			} else { // construct it
				auto& storage = storagePools[mask];
				storage.chunks.emplace_back();

				for (int i = 0; i < metadataPool.size(); i++)
					if (mask.test(i))
						storage.metadata.push_back(&metadataPool.at(i));

				for (auto metadata : storage.metadata)
					storage.entitySize += metadata->size;

				storage.maxNumberOfEntitiesPerChunk = CHUNK_SIZE / storage.entitySize;
			}
		}

	private:

		template <typename T> void defaultConstructorInstace(T* This) { new (This)T(); }
		template <typename T> void copyConstructorInstace(T* This, const T* That) { new (This)T(*That); }
		template <typename T> void destructorInstance(T* This) { This->~T(); }

		static constexpr inline int CHUNK_SIZE = 2 * 1024; // number of bytes

		using constructor_t = void (*)(void*);
		using copy_constructor_t = void (*)(void*, const void*);
		using destructor_t = void (*)(void*);


		struct ComponentMetadata {
			std::type_index type;
			int16_t size; // aligned at 8 bytes
			int16_t nonAlignedSize;
			bool isTrivial;
			bool isDefaultConstructible;
			bool isCopyable;

		private:
			constructor_t default_constructor = nullptr;
			copy_constructor_t copy_constructor = nullptr;
			destructor_t destructor = nullptr;

		public:
			template <typename T>
			void constructMetadata()
			{
				type = typeid(T);
				nonAlignedSize = sizeof(T);
				if (nonAlignedSize % 8 != 0) // align to 8 bytes
					size = nonAlignedSize + 8 - (nonAlignedSize % 8);

				isTrivial = std::is_trivial_v<T>;

				if (std::is_default_constructible_v<T>) {
					default_constructor = Util::reinterpretCast<constructor_t>(defaultConstructorInstace<T>);
					isDefaultConstructible = true;
				}

				isCopyable = std::is_copy_constructible_v<T>;

				// sau asta? std::is_trivially_copyable_v<T>
				if (!std::is_trivially_copy_constructible_v<T>)
					copy_constructor = Util::reinterpretCast<copy_constructor_t>(copyConstructorInstace<T>);

				if (!std::is_trivially_destructible_v<T>)
					destructor = Util::reinterpretCast<destructor_t>(destructorInstance<T>);
			}

			bool default_construct(void* p)
			{
				if (default_constructor)
					default_constructor(p);
			}

			void copy_construct(void* This, const void* That)
			{
				if (copy_constructor) {
					copy_constructor(This, That);
				} else {
					std::memcpy(This, That, this->size);
				}
			}

			void destruct(void* p)
			{
				if (destructor)
					destructor(p);
			}

		};

		// used for tuple impl in getComponents<Ts...>()
		template <typename T>
		struct internal_pair_type {
			using CompT = T;
			int16_t pos = 0;
			bool found = false;
		};

		struct Chunk {
			Chunk() : buffer(CHUNK_SIZE), numberOfEntities(0) {}

			std::vector<byte> buffer;
			int numberOfEntities;
		};

		struct ArchetypeStorage {
			int entitySize; // sum of sizes of component types in this archetype
			int maxNumberOfEntitiesPerChunk;
			std::vector<ComponentMetadata*> metadata;
			std::vector<Chunk> chunks;
		};

		struct HandleData {
			std::type_index type;
			void* pComponent;
			int counter;
		};

		struct FreeSlot {
			int chunkNum;
			int entityNum;
		};

		std::vector<ComponentMetadata> metadataPool;
		std::unordered_map<ArchetypeMask, ArchetypeStorage> storagePools;
		std::unordered_map<ArchetypeMask, std::vector<FreeSlot>> freeSlotPools;
		std::unordered_map<int, std::vector<HandleData>> handlePools; // indexed by entity id
	};



	class EntityManager;

	class Entity {

	public:

		template <typename T, typename... Args>
		T& addComponent(Args&& ... args);

		template <typename T>
		T& getComponent();

		template <typename T1, typename T2>
		std::tuple<T1&, T2&> getComponent();

		template <typename... Ts>
		std::tuple<Ts&...> getComponents(); 

		template <typename T>
		StableReference<T> getComponentStableReference();

		Entity clone();

		template <typename T, typename... Args>
		T* addScript(Args&& ... args);

		template <typename T>
		T* getScript();

		template <typename T>
		void setScriptActive(bool);

		template <typename T>
		void removeScript();

		void setName(std::string);
		const std::string& getName();

		Archetype getArchetype();
		ComponentManager::ArchetypeMask getArchetypeMask();

		friend bool operator==(Entity left, Entity right) {
			return left.manager == right.manager && left.id == right.id;
		}

		friend bool operator!=(Entity left, Entity right) {
			if (left.manager == right.manager)
				return left.id != right.id;
			else
				return true;
		}

	private:
		EntityManager* manager = nullptr;
		int id = ArkInvalidID;

		friend class EntityManager;
	};



	class EntityManager {

	public:

		Entity createEntity(std::string name = "")
		{
			Entity e;
			e.manager = this;
			if (!freeEntities.empty()) {
				e.id = freeEntities.back();
				freeEntities.pop_back();
				auto& entity = getEntity(e);
			} else {
				e.id = entities.size();
				auto& entity = entities.emplace_back();
				entity.mask.reset();
				entity.componentIndexes.fill(ArkInvalidIndex);
			}
			auto& entity = getEntity(e);

			if (name.empty())
				entity.name = std::string("entity_") + std::to_string(e.id);
			else
				entity.name = name;

			std::cout << "create entity with name: " << entity.name << std::endl;
			return e;
		}
		void createEntity(const Archetype& archetype, std::string name = "");
		Entity clone(int id);

		template <typename T, typename... Args>
		T* addComponent(int id, Args&& ... args);

		template <typename T>
		T* getComponent(int id);

		template <typename... Ts>
		std::tuple<Ts&...> getComponents(int id);

		const std::string& getName(int id)
		{

		}

		void setName(int id, std::string name)
		{
			
		}

		void destroyEntity(Entity e);

		//// as putea sa fac asa, si fac if constexpr(!is_same<T3, void*>) {...} s.a.m.d.
		//// ar trebui sa returnez nullptr pentru 'void*'
		//template <
		//	typename T1,
		//	typename T2,
		//	typename T3 = void*,
		//	typename T4 = void*,
		//	typename T5 = void*>
		//	std::tuple<T1, T2, T3, T4, T5> getComponents2();


	private:

		struct InternalEntityData {
			ComponentManager::ArchetypeMask mask;
			int16_t chunkNum = ArkInvalidIndex;
			int16_t entityNum = ArkInvalidIndex; // a câta entitate este in chunk
			int16_t scriptsIndex = ArkInvalidIndex;
			//int16_t nameIndex = ArkInvalidIndex;
			std::string name;
		};

		InternalEntityData& getEntity(Entity e)
		{
#ifdef NDEBUG
			return entities[e.id];
#else
			return entities.at(e.id);
#endif
		}


		ComponentManager& componentManager;
		ScriptManager& scriptManager;

		std::vector<InternalEntityData> entities;
		std::vector<std::string> names;
		std::vector<int> freeEntities;
	};

	template <typename T, typename ...Args>
	inline T& Entity::addComponent(Args&& ...args)
	{
		//static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
		return manager->addComponent<T>(this->id, std::forward<Args>(args)...);
	}

	template <typename T>
	inline T& Entity::getComponent()
	{
		//static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
		return manager->getComponent<T>(this->id);
	}

	template <typename T1, typename T2>
	inline std::tuple<T1&, T2&> Entity::getComponent()
	{
		//static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
		return manager->getComponent<T1, T2>(this->id);
	}

	template <typename... Ts>
	inline std::tuple<Ts&...> Entity::getComponents()
	{
		//static_assert(std::is_base_of_v<Component, T>, " T is not a Component");
		return manager->getComponent<T1, T2>(this->id);
	}

	template <typename T>
	StableReference<T> Entity::getComponentStableReference()
	{
		return manager->getComponentStableRefernce(this->id);
	}

	template <typename T, typename ...Args>
	inline T* Entity::addScript(Args&& ...args)
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		return manager->addScript<T>(this->id, std::forward<Args>(args)...);
	}

	template <typename T>
	inline T* Entity::getScript()
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		return manager->getScript<T>(this->id);
	}

	template <typename T>
	inline void Entity::setScriptActive(bool active)
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		return manager->setScriptActive<T>(this->id, active);
	}

	template <typename T>
	inline void Entity::removeScript()
	{
		static_assert(std::is_base_of_v<Script, T>, " T is not a Script");
		return manager->removeScript<T>(this->id);
	}

	inline Entity Entity::clone()
	{
		return manager->clone(this->id);
	}

	inline void Entity::setName(std::string name)
	{
		manager->setName(this->id, name);
	}

	inline const std::string& Entity::getName()
	{
		return manager->getName(this->id);
	}
}
