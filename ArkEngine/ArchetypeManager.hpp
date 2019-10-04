#pragma once

#include "Core.hpp"

#include <bitset>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <type_traits>
#include <typeindex>

// future entity archetype manager to write
// components of a single entity are stored in the same buffer, as oposed to storing them in pools

namespace Ark {

	static inline constexpr std::size_t MaxComponentTypes = 64;

	//using Archetype = const ArchetypeMask&;

	// component must be POD type
	class ArchetypeComponentManger {

	public:

		using ArchetypeMask = std::bitset<MaxComponentTypes>;

	private:
		template <typename T>
		void addType()
		{
			if (!hasComponentType<T>())
				addComponentType<T>();
		}


		template <typename T>
		bool hasComponentType()
		{
			auto pos = std::find(std::begin(this->types), std::end(this->types), typeid(T));
			return pos != std::end(this->types);
		} 

		int getComponentId(std::type_index type)
		{
			auto pos = std::find(std::begin(this->types), std::end(this->types), type);
			if (pos == std::end(this->types)) {
				std::cout << "component manager dosent have component type: " << type.name();
				return -1;
			}
			return pos - std::begin(this->types);
		}

		template <typename T>
		int getComponentId()
		{
			getComponentId(typeid(T));
		}

		//static std::string getComponentNames()
		//{
		//}

	private:

		template <typename T>
		void addComponentType()
		{
			if (types.size() == MaxComponentTypes) {
				std::cerr << "nu mai e loc de tipuri de componente, max: " << MaxComponentTypes << std::endl;
				return;
			}
			types.push_back(typeid(T));
		}

	private:

		template <typename T>
		void assignerInstance(T* This, T* From) { *This = *From; }

		template <typename T>
		void destructorInstance(T* This) { This->~T(); }

		using assign_t = void (*)(void*, void*);
		using destructor_t = void (*)(void*);

#if 0
		template <typename T>
		assign_t getAssignFunc()
		{
			// using union to erase the type
			union {
				void (*ret)(void*, void*);
				void (*assign)(T*, T*);
			}u;

			u.assign = assignFunc<T>;
			return u.ret;
		}
#endif

		template <typename T>
		assign_t getAssignFunc()
		{
			return Util::reinterpretCast<assign_t>(assignerInstance<T>);
		}

		template <typename T>
		destructor_t getDestructor()
		{
			return Util::reinterpretCast<destructor_t>(destructorInstance<T>);
		}

		static constexpr inline int chunkSizeInBytes = 2 * 1024;

		using Chunk = std::vector<uint8_t>;

		struct ComponentMetadata {
			std::type_index type;
			assign_t assign;
			destructor_t dtor;
			int size;
			bool isPOD;
		};

		struct ArchetypeStorage {
			int entitySize; // sum of sizes of component types in this archetype
			std::vector<ComponentMetadata> metadata;
			std::vector<Chunk> chunks;
		};

		struct PendingComponentData {
			std::vector<int8_t> buffer; // temporary component storage
			std::vector<ComponentMetadata> componentMetadata;
			ArchetypeMask archetype;
		};

		std::vector<std::type_index> types;
		//std::vector<std::any> componentTemplates; // has an instance defautl constructed of each component type, used for cloning
		std::unordered_map<ArchetypeMask, ArchetypeStorage> pools;
		std::unordered_map<ArchetypeMask, std::vector<int>> freeSlots;
		std::vector<PendingComponentData> pendingComponents;

		friend class ArchetypeEntityManager;
	};


	class ArchetypeEntityManager;
	//class ArchetypeEntityManager::EntityData;


	class Entity {

	public:

	private:
		ArchetypeEntityManager& manager;
		//ArchetypeEntityManager::EntityData& data;
		friend class ScriptManager;
		friend class ArchetypeEntityManager;
	};


	class ArchetypeEntityManager {

	public:


		//template <typename T>
		//bool hasComponentType()

		//template <typename T>
		//void addComponentType()

		//template <typename T>
		//int getComponentId()

		//template <typename T>
		//ComponentContainer<T>& getComponentPool()

		template <typename T, typename... Args>
		T* addComponentOnEntity(Entity& entity, Args&& ... args);

		template <typename T>
		T* getComponentOfEntity(Entity& entity);

		template <typename... Ts>
		std::tuple<Ts...> getComponents(); // TODO: cum plm o implementez pe asta getComponents->std::tuple<ts...>?

		// as putea sa fac asa, si fac if constexpr(!is_same<T3, void*>) {...} s.a.m.d.
		// ar trebui sa returnez nullptr pentru 'void*'
		template <
			typename T1, 
			typename T2, 
			typename T3=void*, 
			typename T4=void*, 
			typename T5=void*>
		std::tuple<T1, T2, T3, T4, T5> getComponents2(); 

		// sau...
		template <typename T1, typename T2>
		std::tuple<T1, T2> getComponents3();

		// s.a.m.d. ...
		template <typename T1, typename T2, typename T3>
		std::tuple<T1, T2, T3> getComponents3();

	private:

		struct EntityData {
			ComponentManager::ComponentMask mask;
			std::vector<int8_t> buffer; // component storage
			struct ComponentMetadata {
				std::type_index type;
				int size;
			};
			std::vector<ComponentMetadata> componentMetadata;
		};

		//struct EntitiesPool {
		//	Detail::ComponentMask componentMask;
		//	std::vector<int8_t> buffer;
		//	std::deque<EntityData> pool;
		//};
		//std::vector<EntitiesPool> pools;

		std::unordered_map<ComponentManager::ComponentMask, std::deque<EntityData>> pools;

		struct PendingEntity {
			EntityData entityData;
			ComponentManager::ComponentMask componentMask;
		};
		std::vector<PendingEntity> pendingEntities;
	};
}
