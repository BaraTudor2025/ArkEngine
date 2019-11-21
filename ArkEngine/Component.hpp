#pragma once

#include "Core.hpp"
#include "Util.hpp"

#include <libs/static_any.hpp>
#include <libs/Meta.h>

#include <bitset>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <functional>
#include <any>

template <typename T>
struct ARK_ENGINE_API Component {
	Component()
	{
		static_assert(std::is_default_constructible_v<T>, "Component needs to have default constructor");
	}
};

namespace ImGui {
	void PushID(int);
	void PopID();
	bool BeginCombo(const char* label, const char* preview_value, int flags);
	void EndCombo();
	void SetItemDefaultFocus();
	void Indent(float);
	void Unindent(float);
	void Text(char const*, ...);
}

extern void ArkSetFieldName(std::string_view name);
extern bool ArkSelectable(const char* label, bool selected); // forward to ImGui::Selectable

class ComponentManager final : NonCopyable {

public:
	using byte = std::byte;
	static inline constexpr std::size_t MaxComponentTypes = 32;
	using ComponentMask = std::bitset<MaxComponentTypes>;

	bool hasComponentType(std::type_index type)
	{
		auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
		return pos != std::end(this->componentIndexes);
	} 

	int getComponentId(std::type_index type)
	{
		auto pos = std::find(std::begin(this->componentIndexes), std::end(this->componentIndexes), type);
		if (pos == std::end(this->componentIndexes)) {
			EngineLog(LogSource::ComponentM, LogLevel::Critical, "type not found (%s) ", Util::getNameOfType(type));
			return ArkInvalidIndex;
		}
		return pos - std::begin(this->componentIndexes);
	}

	std::type_index getTypeFromId(int id)
	{
		return componentIndexes.at(id);
	}

	template <typename T>
	int getComponentId()
	{
		return getComponentId(typeid(T));
	}

	auto addComponent(int compId) -> std::pair<byte*, int>
	{
		auto& pool = pools.at(compId);
		auto [component, index] = pool.getFreeSlot();
		pool.metadata.default_construct(component);
		return {component, index};
	}

	auto copyComponent(int compId, int index) -> std::pair<byte*, int>
	{
		auto& pool = pools.at(compId);
		return pool.copyComponent(index);
	}

	void removeComponent(int compId, int index)
	{
		auto& pool = pools.at(compId);
		pool.removeComponent(index);
	}

	template <typename T>
	void addComponentType()
	{
		static_assert(std::is_base_of_v<Component<T>, T>, " T is not a Component");
		const auto& type = typeid(T);
		if (hasComponentType(type))
			return;

		EngineLog(LogSource::ComponentM, LogLevel::Info, "adding type (%s)", Util::getNameOfType(type));
		if (componentIndexes.size() == MaxComponentTypes) {
			EngineLog(LogSource::ComponentM, LogLevel::Error, "aborting... nr max of components is &d, trying to add type (%s), no more space", MaxComponentTypes, Util::getNameOfType(type));
			std::abort();
		}
		componentIndexes.push_back(type);
		pools.emplace_back();
		pools.back().constructPoolFrom<T>();
	}

	void renderEditorOfComponent(int* widgetId, int compId, void* component)
	{
		auto& pool = pools.at(compId);
		pool.metadata.renderFields(widgetId, component);
	}

	std::vector<std::type_index>& getTypes() {
		return this->componentIndexes;
	}


private:

	using FieldFunc = std::function<std::any(std::string_view, const void*)>;
	static std::unordered_map<std::type_index, FieldFunc> fieldRendererTable;

	struct Metadata {
		std::string_view name;
		int size;
		bool isCopyable = false;
		std::function<void(int*, void*)> renderFields; // render in editor
		//std::function<json(void*)> serialize;
		//std::function<void(void*, const json*)> deserialize;

		Metadata() = default;

		void default_construct(void* p) {
			default_constructor(p);
		}

		void copy_construct(void* This, const void* That) {
			if (copy_constructor)
				copy_constructor(This, That);
		}

		void destruct(void* p) {
			if (destructor)
				destructor(p);
		}

		template <typename T>
		static bool renderEnumFields(T& currentValue)
		{
			static_assert(std::is_enum_v<T>);
			const auto& fields = meta::registerEnum<T>();

			const char* currFieldName = nullptr;
			for (const auto& field : fields)
				if (field.value == currentValue)
					currFieldName = field.name;

			if (ImGui::BeginCombo("", currFieldName, 0)) {
				for (const auto& field : fields) {
					if (ArkSelectable(field.name, field.value == currentValue)) {
						ImGui::EndCombo();
						currentValue = field.value;
						return true;
					}
					if (field.value == currentValue)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			return false;
		}

		template <typename T>
		static bool renderFieldsOfType(int* widgetId, void* pValue){
			T& valueToRender = *static_cast<T*>(pValue);
			std::any newValue;
			bool modified = false;

			meta::doForAllMembers<T>([widgetId, &modified, &newValue, &valueToRender, &table = fieldRendererTable](auto& member) mutable {
				using MemberT = meta::get_member_type<decltype(member)>;
				ImGui::PushID(*widgetId);
				if constexpr (meta::isRegistered<MemberT>()) {
					// recursively render members
					auto value = member.getCopy(valueToRender);
					ImGui::Text("%s:", member.getName());
					if (renderFieldsOfType<MemberT>(widgetId, &value)) {
						member.set(valueToRender, value);
						modified = true;
					}
				} else if constexpr (std::is_enum_v<MemberT>) {
					// render enum values
					auto value = member.getCopy(valueToRender);
					ArkSetFieldName(member.getName());
					if (renderEnumFields(value)) {
						member.set(valueToRender, value);
						modified = true;
					}
				} else {
					// render field using predefined table
					auto& renderField = table.at(typeid(MemberT));

					if (member.canGetConstRef())
						newValue = renderField(member.getName(), &member.get(valueToRender));
					else /* if (member.hasGetter()) /**/ {
						MemberT local = member.getCopy(valueToRender);
						newValue = renderField(member.getName(), &local);
					}

					if (newValue.has_value()) {
						if (member.hasSetter()) {
							member.set(valueToRender, std::any_cast<MemberT>(newValue));
							modified = true;
						} else if (member.canGetRef()) {
							member.getRef(valueToRender) = std::any_cast<MemberT>(newValue);
							modified = true;
						}
					}
					newValue.reset();
				}
				ImGui::PopID();
				*widgetId += 1;
			});
			return modified;
		}

		template <typename T>
		void constructMetadataFrom()
		{
			name = Util::getNameOfType<T>();

			constexpr int nBytes = 16;
			size = sizeof(T);
			// align to nBytes
			if (size < nBytes)
				size = nBytes;
			else if (size % nBytes != 0)
				size = size + nBytes - (size % nBytes);
			// else size is already aligned

			default_constructor = Util::reinterpretCast<constructor_t>(defaultConstructorInstace<T>);

			if constexpr (std::is_copy_constructible_v<T>) {
				copy_constructor = Util::reinterpretCast<copy_constructor_t>(copyConstructorInstace<T>);
				isCopyable = true;
			}

			if constexpr (!std::is_trivially_destructible_v<T>)
				destructor = Util::reinterpretCast<destructor_t>(destructorInstance<T>);

			renderFields = renderFieldsOfType<T>;
		}
		
	private:
		template <typename T> static void defaultConstructorInstace(T* This) { new (This)T(); }
		template <typename T> static void copyConstructorInstace(T* This, const T* That) { new (This)T(*That); }
		template <typename T> static void destructorInstance(T* This) { This->~T(); }

		using constructor_t = void (*)(void*);
		using copy_constructor_t = void (*)(void*, const void*);
		using destructor_t = void (*)(void*);

		constructor_t default_constructor = nullptr;
		copy_constructor_t copy_constructor = nullptr;
		destructor_t destructor = nullptr;
	};


	class ComponentPool {

	public:

		template <typename T>
		void constructPoolFrom()
		{
			metadata.constructMetadataFrom<T>();
			int kiloByte = 2 * 1024;
			numberOfComponentsPerChunk = kiloByte / metadata.size;
			chunk_size = metadata.size * numberOfComponentsPerChunk;
			EngineLog(LogSource::ComponentM, LogLevel::Info, "for (%s), chunkSize (%d), compNumPerChunk(%d), compSize(%d) ", Util::getNameOfType<T>(), chunk_size, numberOfComponentsPerChunk, metadata.size);
		}

		auto copyComponent(int index) -> std::pair<byte*, int>
		{
			auto [newComponent, newIndex] = getFreeSlot();
			byte* component = getComponent(index);
			if (metadata.isCopyable)
				metadata.copy_construct(newComponent, component);
			else
				metadata.default_construct(newComponent);
			return {newComponent, newIndex};
		}

		void removeComponent(int index)
		{
			byte* component = getComponent(index);
			metadata.destruct(component);
			freeComponents.push_back(index);
		}

		std::pair<int, int> getNums(int index)
		{
			int chunkNum = index / numberOfComponentsPerChunk;
			int componentNum = index % numberOfComponentsPerChunk;
			return {chunkNum, componentNum};
		}

		byte* getComponent(int index)
		{
			auto [chunkNum, componentNum] = getNums(index);
			auto& chunk = chunks.at(chunkNum);
			byte* component = &chunk.buffer.at(componentNum * metadata.size);
			return component;
		}

		auto getFreeSlot() -> std::pair<byte*, int>
		{
			if (freeComponents.empty()) {
				if (chunks.empty())
					chunks.emplace_back(chunk_size);
				if (chunks.back().numberOfComponents == numberOfComponentsPerChunk) {
					chunks.emplace_back(chunk_size);
				}
				auto& chunk = chunks.back();
				int index = numberOfComponentsPerChunk * (chunks.size() - 1) + chunk.numberOfComponents;
				byte* component = getComponent(index);
				chunk.numberOfComponents += 1;
				return {component, index};
			} else {
				int index = freeComponents.back();
				freeComponents.pop_back();
				return {getComponent(index), index};
			}
		}

		struct Chunk {
			Chunk(int CHUNK_SIZE) : buffer(CHUNK_SIZE), numberOfComponents(0) { }
			int numberOfComponents;
			std::vector<byte> buffer;
		};

	public:
		Metadata metadata;
	private:
		int numberOfComponentsPerChunk;
		int chunk_size;
		std::vector<Chunk> chunks;
		std::vector<int> freeComponents;
	};


	friend class System;
	//std::unordered_map<std::type_index, ComponentPool> pools;
	std::vector<std::type_index> componentIndexes;
	std::vector<ComponentPool> pools;
};
