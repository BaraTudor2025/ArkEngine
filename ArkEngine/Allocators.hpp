#pragma once

#include <memory>
#include <memory_resource>
#include <functional>
#include <deque>
#include <ark/util/Util.hpp>

constexpr std::size_t operator"" _KiB(unsigned long long value) noexcept
{
	return std::size_t(value * 1024);
}

constexpr std::size_t operator"" _KB(unsigned long long value) noexcept
{
	return std::size_t(value * 1000);
}

constexpr std::size_t operator"" _MiB(unsigned long long value) noexcept
{
	return std::size_t(value * 1024 * 1024);
}

constexpr std::size_t operator"" _MB(unsigned long long value) noexcept
{
	return std::size_t(value * 1000 * 1000);
}

template <typename T, typename...Args>
inline auto makeObjectFromResource(std::pmr::memory_resource* res, Args&&... args) -> T* {
	void* ptr = res->allocate(sizeof(T), alignof(T));
	return std::construct_at<T>(ptr, std::forward<Args>(args)...);
}
template <typename T>
inline void destroyObjectFromResource(std::pmr::memory_resource* res, T* ptr)  {
	std::destroy_at(ptr);
	res->deallocate(ptr, sizeof(T), alignof(T));
}

struct PmrResourceDeleter {
	std::pmr::memory_resource* res;

	template <typename T>
	void operator()(T* ptr) {
		destroyObjectFromResource(res, ptr);
	}
};

template <typename T, typename...Args>
inline auto makeUniqueFromResource(std::pmr::memory_resource* res, Args&&...args) -> std::unique_ptr<T, PmrResourceDeleter>
{
	void* vptr = res->allocate(sizeof(T), alignof(T));
	T* ptr = new (vptr) T(std::forward<Args>(args)...);
	return std::unique_ptr<T, PmrResourceDeleter>(ptr, { res });
}

// delay insertion so it can be done at once
// dar mai bine dau un 'reserve' SI GATA
template <typename V>
struct VectorBuilderWithResource {

	std::vector<std::function<void()>> builder;
	std::pmr::memory_resource* res;
	V& toBuild;

	VectorBuilderWithResource(V& toBuild, std::pmr::memory_resource* res) : toBuild(toBuild), res(res) {}
	VectorBuilderWithResource(V& toBuild) : toBuild(toBuild), res(toBuild.get_allocator().resource()) {}

	template <typename T, typename...Args>
	void add(Args&&... args) {
		builder.emplace_back([this, &args...]() {
			void* vptr = res->allocate(sizeof(T), alignof(T));
			//T* ptr = new (vptr) T(std::forward<Args>(args)...);
			T* ptr = new (vptr) T(std::move(args)...);
			toBuild.emplace_back(ptr);
		});
	}

	template <typename T, typename...Args>
	void addUnique(Args&&... args) {
		builder.emplace_back([this, &args...]() {
			toBuild.emplace_back(makeUniqueFromResource<T>(res, std::move(args)...));
			//toBuild.emplace_back(makeUniqueFromResource<T>(res, std::forward<Args>(args)...));
		});
	}

	void build() {
		toBuild.reserve(builder.size());
		for (auto& f : builder) {
			f();
		}
	}
};

// needs to be pmr
template <typename T>
struct ContainerResourceDeleterGuard {
	T& vec;
	ContainerResourceDeleterGuard(T& vec) : vec(vec) {}

	~ContainerResourceDeleterGuard() {
		for (auto* elem : vec)
			destroyObjectFromResource(vec.get_allocator().resource(), elem);
	}
};


// basically a ring-buffer, allocate TEMPORARY objects(no more than a few frames)
// when the it reaches the end of the buffer then loop from the beginning,
// if objects are still alive on loop they will get rewriten, allocate bigger buffer next time :P
class TemporaryResource : public std::pmr::memory_resource {
	std::unique_ptr<std::byte[]> memory;
	size_t size;
	void* ptr;
	size_t remainingSize;
	struct {
		void* ptr;
		size_t bytes;
	} lastDeallocation;
public:
	TemporaryResource(size_t size) 
		: memory(new std::byte[size]), ptr(memory.get())
		, size(size), remainingSize(size) { }

	void* do_allocate(const size_t bytes, const size_t align) override {
		if (std::align(align, bytes, ptr, remainingSize)) {
			this->remainingSize -= bytes;
			void* mem = ptr;
			ptr = (std::byte*)ptr + bytes;
			return mem;
		}
		else {
			if (remainingSize == size)
				return nullptr;
			ptr = memory.get();
			remainingSize = size;
			return do_allocate(bytes, align);
		}
	}

	// lol
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		lastDeallocation = { p, bytes };
	}

	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

class SegregatorResource : public std::pmr::memory_resource {
	std::pmr::memory_resource* small;
	std::pmr::memory_resource* large;
	int threshold;
public:
	SegregatorResource(int threshold, std::pmr::memory_resource* small, std::pmr::memory_resource* large)
		: small(small), large(large), threshold(threshold) { }

	void* do_allocate(const size_t bytes, const size_t align) override {
		if (bytes <= threshold)
			return small->allocate(bytes, align);
		else
			return large->allocate(bytes, align);
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		if (bytes <= threshold)
			small->deallocate(p, bytes, align);
		else
			large->deallocate(p, bytes, align);
	}
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

class AffixAllocator : public std::pmr::memory_resource {
	std::pmr::memory_resource* upstream;

public:
	struct Callback {
		// pointer, size, align allocate=true, dealloc=false
		std::function<void(size_t, size_t, bool)> prefix;
		std::function<void(void*, size_t, size_t, bool)> postfix;
	}cbs;

	AffixAllocator(std::pmr::memory_resource* up, Callback cb)
		: upstream(up), cbs(cb) {}

	void* do_allocate(const size_t bytes, const size_t align) override {
		if(cbs.prefix)
			cbs.prefix(bytes, align, true);
		void* p = upstream->allocate(bytes, align);
		if(cbs.postfix)
			cbs.postfix(p, bytes, align, true);
		return p;
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		if(cbs.prefix)
			cbs.prefix(bytes, align, false);
		upstream->deallocate(p, bytes, align);
		if(cbs.postfix)
			cbs.postfix(nullptr, bytes, align, false);
	}
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

inline auto makePrintingMemoryResource(std::string name, std::pmr::memory_resource* upstream) -> AffixAllocator 
{
	auto printing_prefix = [name](size_t bytes, size_t, bool alloc) {
		if (alloc)
			std::printf("alloc(%s) : %zu bytes\n", name.c_str(), bytes);
		else
			std::printf("dealloc(%s) : %zu bytes\n", name.c_str(), bytes);
	};
	return AffixAllocator(upstream, {printing_prefix});
}


class TrackingResource : public std::pmr::memory_resource {

};

class FallBackResource : public std::pmr::memory_resource {
	std::pmr::memory_resource* primary;
	std::pmr::memory_resource* secondary;
	std::deque<void*> secondaryPtrs;

public:
	FallBackResource(std::pmr::memory_resource* p, std::pmr::memory_resource* s)
		: primary(p), secondary(s) { }
	
	void* do_allocate(const size_t bytes, const size_t align) override {
		void* p = primary->allocate(bytes, align);
		if (!p) {
			p = secondary->allocate(bytes, align);
			secondaryPtrs.push_back(p);
		}
		return p;
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		if (secondaryPtrs.empty() || secondaryPtrs.end() == std::find(secondaryPtrs.begin(), secondaryPtrs.end(), p)) {
			primary->deallocate(p, bytes, align);
			std::erase(secondaryPtrs, p);
		}
		else
			secondary->deallocate(p, bytes, align);
	}
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

// doesn't call destructor
template <typename T>
class WinkOut {
	std::aligned_storage_t<sizeof(T), alignof(T)> storage;
	T* _ptr() { return reinterpret_cast<T*>(&storage); }

public:
	template <typename... Args>
	WinkOut(Args&&... args) {
		new (&storage) T(std::forward<Args>(args)...);
	}

	T* data() { return _ptr(); }

	T& operator*() {
		return *_ptr();
	}

	T* operator->() {
		return _ptr();
	}

	void destruct() {
		_ptr()->~T();
	}
};

