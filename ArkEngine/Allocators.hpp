#pragma once

#include <memory>
#include <memory_resource>
#include <functional>
#include <deque>
#include <chrono>
#include <ark/util/Util.hpp>
#include <ark/core/Logger.hpp>

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
	return new (ptr) T(std::forward<Args>(args)...);
	//return std::construct_at<T>(ptr, std::forward<Args>(args)...);
}
template <typename T>
inline void destroyObjectFromResource(std::pmr::memory_resource* res, T* ptr)  {
	//std::destroy_at(ptr);
	ptr->~T();
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
	return std::unique_ptr<T, PmrResourceDeleter>(makeObjectFromResource<T>(res, std::forward<Args>(args)...), { res });
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

class impl_equal_std_resource : public std::pmr::memory_resource {
public:
	bool do_is_equal(const std::pmr::memory_resource& mem) const noexcept override {
		return this == &mem;
	}
};

// basically a ring-buffer, allocate TEMPORARY objects(valid only one frame)
class TemporaryResource : public impl_equal_std_resource {
	std::unique_ptr<std::byte[]> m_memory;
	const size_t mk_size;
	std::byte* m_ptr;
	size_t m_remainingSize;
	decltype(m_ptr) m_lastAlloc = nullptr;
public:
	TemporaryResource(size_t size) 
		: m_memory(new std::byte[size]), 
		m_ptr(m_memory.get()),
		mk_size(size), m_remainingSize(size) { }

	void* do_allocate(const size_t bytes, const size_t align) override {
		if (std::align(align, bytes, (void*&)m_ptr, m_remainingSize)) {
			void* mem = m_ptr;
			m_lastAlloc = m_ptr;
			m_remainingSize -= bytes;
			m_ptr += bytes;
			return mem;
		}
		else {
			this->reset();
			return do_allocate(bytes, align);
		}
	}

	void reset() {
		m_ptr = m_memory.get();
		m_remainingSize = mk_size;
	}

	void do_deallocate(void* p, size_t bytes, size_t align) override {
		// dealoca doar daca asta-i ultimul obiect alocat, lol
		if (m_lastAlloc == p) {
			m_ptr -= bytes;
			m_remainingSize += bytes;
			m_lastAlloc = nullptr;
		}
	}
};

template <typename T>
auto makeTempVector(int size = 0) -> std::pmr::vector<T>
{
	static TemporaryResource c_resource(1_MiB);
	std::pmr::vector<T> vec(&c_resource);
	vec.reserve(size);
	return vec;
	//return std::pmr::vector<T>(size, &c_resource);
}


class SegregatorResource : public impl_equal_std_resource {
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
};


class AffixResource : public impl_equal_std_resource {
	std::pmr::memory_resource* upstream;

public:
	struct Callback {
		// pointer, size, align allocate=true, dealloc=false
		std::function<void(size_t, size_t, bool)> prefix;
		std::function<void(void*, size_t, size_t, bool)> postfix;
	}cbs;

	AffixResource(std::pmr::memory_resource* up, Callback cb)
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
};

class MallocResource : public impl_equal_std_resource {
public:
	void* do_allocate(const size_t bytes, const size_t align) override {
		//return _aligned_malloc(bytes, align);
		return std::malloc(bytes);
	}
	void do_deallocate(void* p, size_t bytes, size_t align) override {
		//_aligned_free(p);
		std::free(p);
	}
};

#if 0

template <class Allocator, unsigned MinSize, unsigned MaxSize, unsigned StepSize>
class bucketizer {
public:
	static constexpr bool supports_truncated_deallocation = false;
	static constexpr unsigned alignment = Allocator::alignment;

	static_assert(MinSize < MaxSize, "MinSize must be smaller than MaxSize");
	static_assert((MaxSize - MinSize + 1) % StepSize == 0, "Incorrect ranges or step size!");

	static constexpr unsigned number_of_buckets = ((MaxSize - MinSize + 1) / StepSize);
	static constexpr unsigned max_size = MaxSize;
	static constexpr unsigned min_size = MinSize;
	static constexpr unsigned step_size = StepSize;

	using allocator = Allocator;

	Allocator _buckets[number_of_buckets];

	bucketizer() noexcept
	{
		for (size_t i = 0; i < number_of_buckets; i++) {
			_buckets[i].set_min_max(MinSize + i * StepSize, MinSize + (i + 1) * StepSize - 1);
		}
	}

	static constexpr size_t good_size(size_t n) noexcept {
		return (number_of_buckets - (max_size - n) / step_size + 1) * step_size;
	}

	/**
	 * Allocates the requested number of bytes. The request is forwarded to
	 * the bucket with which edges are at [min,max] bytes.
	 * \param n The number of bytes to be allocated
	 * \return The Block describing the allocated memory
	 */
	block allocate(size_t n) noexcept
	{
		size_t i = 0;
		while (i < number_of_buckets) {
			if (_buckets[i].min_size() <= n && n <= _buckets[i].max_size()) {
				return _buckets[i].allocate(n);
			}
			++i;
		}
		return{};
	}

	/**
	 * Checks, if the given block is owned by one of the bucket item
	 * \param b The block to be checked
	 * \return Returns true, if the block is owned by one of the bucket items
	 */
	bool owns(const block& b) const noexcept
	{
		return b && (MinSize <= b.length && b.length <= MaxSize);
	}

	/**
	 * Forwards the reallocation of the given block to the corresponding bucket
	 * item.
	 * If the length of the given block and the specified new size crosses the
	 * boundary of a bucket, then content memory of the block is moved to the
	 * new bucket item
	 * \param b Then  Block its size should be changed
	 * \param n The new size of the block.
	 * \return True, if the reallocation was successful.
	 */
	bool reallocate(block& b, size_t n) noexcept
	{
		if (n != 0 && (n < MinSize || n > MaxSize)) {
			return false;
		}

		if (internal::is_reallocation_handled_default(*this, b, n)) {
			return true;
		}

		assert(owns(b));

		const auto alignedLength = internal::round_to_alignment(StepSize, n);
		auto currentAllocator = find_matching_allocator(b.length);
		auto newAllocator = find_matching_allocator(alignedLength);

		if (currentAllocator == newAllocator) {
			return true;
		}

		return internal::reallocate_with_copy(*currentAllocator, *newAllocator, b, alignedLength);
	}

	/**
	 * Frees the given block and resets it.
	 * \param b The block, its memory should be freed
	 */
	void deallocate(block& b) noexcept
	{
		if (!b) {
			return;
		}
		if (!owns(b)) {
			assert(!"It is not wise to let me deallocate a foreign Block!");
			return;
		}

		auto currentAllocator = find_matching_allocator(b.length);
		currentAllocator->deallocate(b);
	}

	/**
	 * Deallocates all resources. Beware of possible dangling pointers!
	 * This method is only available if Allocator::deallocate_all is available
	 */
	template <typename U = Allocator>
	typename std::enable_if<traits::has_deallocate_all<U>::value, void>::type
		deallocate_all() noexcept
	{
		for (auto& item : _buckets) {
			traits::AllDeallocator<U>::do_it(item);
		}
	}

private:
	Allocator* find_matching_allocator(size_t n) noexcept
	{
		assert(MinSize <= n && n <= MaxSize);
		auto v = alb::internal::round_to_alignment(StepSize, n);
		return &_buckets[(v - MinSize) / StepSize];
	}
};

#endif

//template <class Allocator, unsigned MinSize, unsigned MaxSize, unsigned StepSize>
//const unsigned bucketizer<Allocator, MinSize, MaxSize, StepSize>::number_of_buckets;
//  }

inline auto makePrintingMemoryResource(std::string name, std::pmr::memory_resource* upstream) -> AffixResource 
{
	auto printing_prefix = [name](size_t bytes, size_t, bool alloc) {
		if (alloc)
			std::printf("alloc(%s) : %zu bytes\n", name.c_str(), bytes);
		else
			std::printf("dealloc(%s) : %zu bytes\n", name.c_str(), bytes);
	};
	return AffixResource(upstream, {.prefix = printing_prefix});
}

template<typename ... Args>
static auto string_format_temp(std::string_view format, const Args& ... args)
{
	//int size = std::snprintf(nullptr, 0, format.data(), args...) + 1;  // Extra space for '\0'
	//if (size <= 0) { throw std::runtime_error("Error during formatting."); }
	//std::unique_ptr<char[]> buf(new char[size]);
	//std::snprintf(buf.get(), size, format.data(), args ...);
	//return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	//return std::string(buf.get());

	static TemporaryResource temp(5 * 1024);
	int size = 1024;
	char* buf = (char*)temp.allocate(size, 1);
	int n = std::snprintf(buf, size, format.data(), args...);
	return std::pmr::string(buf, n,  &temp); // We don't want the '\0' inside
}

class TrackingResource : public impl_equal_std_resource {

	struct Log {
		std::chrono::steady_clock::time_point allocTime;
		std::chrono::steady_clock::time_point deallocTime;
		int bytes;
		int align;
		void* pointer;
		
		//auto time() const -> std::chrono::milliseconds {
		auto time() const -> std::int32_t {
			return std::chrono::duration_cast<std::chrono::milliseconds>(deallocTime - allocTime).count();
		}
	};

	std::pmr::memory_resource* m_upstream;
	std::pmr::vector<Log> m_logs;
	std::pmr::vector<Log> m_activeLogs;

	Log* findLog(void* ptr, decltype(m_logs)& logs) {
		auto it = std::find_if(logs.begin(), logs.end(), [&](const Log& log) {return log.pointer == ptr; });
		if (it != logs.end())
			return &*it;
		else
			return nullptr;
	}

public:
	TrackingResource(std::pmr::memory_resource* upstream) 
		: m_upstream(upstream), m_logs(upstream), m_activeLogs(upstream) {}

	void* do_allocate(const size_t bytes, const size_t align) override {
		void* ptr = m_upstream->allocate(bytes, align);
		Log& log = m_activeLogs.emplace_back();
		log.align = align;
		log.bytes = bytes;
		log.pointer = ptr;
		log.allocTime = std::chrono::steady_clock::now();
		return ptr;
	}

	void do_deallocate(void* p, size_t bytes, size_t align) override {
		m_upstream->deallocate(p, bytes, align);
		if (Log* log = findLog(p, m_activeLogs)) {
			log->deallocTime = std::chrono::steady_clock::now();
			m_logs.push_back(*log);
			std::erase_if(m_activeLogs, [&](const Log& log) { return log.pointer == p; });
		}
	}

	void clearLogs() {
		m_logs.clear();
		m_activeLogs.clear();
	}

	float asKilo(float bytes) const {
		return float(bytes) / 1024;
	}
	float asMega(float bytes) const {
		return asKilo(bytes) / 1024;
	}

	struct Bytes {
		int total = 0;
		int active = 0;
		int reclaimed = 0;
	};

	// returns string
	std::string formatBytes(int bytes) const {
		char c_buff[10];
		if (bytes > 1024 * 1024)
			sprintf_s(c_buff, sizeof(c_buff), "%.2f Mb", asMega(bytes));
		else if (bytes > 1024)
			sprintf_s(c_buff, sizeof(c_buff), "%.2f Kb", asKilo(bytes));
		else
			sprintf_s(c_buff, sizeof(c_buff), "%d B", bytes);
		return c_buff;
	}

	Bytes getBytes() const {
		Bytes bytes;
		for (const Log& log : m_logs) {
			bytes.reclaimed += log.bytes;
		}
		for (const Log& log : m_activeLogs) {
			bytes.active += log.bytes;
		}
		bytes.total = bytes.reclaimed + bytes.active;
		return bytes;
	}

	// returns string
	auto formatSummary() const {
		Bytes bytes = getBytes();
		const auto total = formatBytes(bytes.total);
		const auto active = formatBytes(bytes.active);
		const auto recl = formatBytes(bytes.reclaimed);
		auto str = string_format_temp("Allocations %d, %s, (active %d, %s), (deallocated %d, %s)\n", 
			m_logs.size() + m_activeLogs.size(), total.c_str(), 
			m_activeLogs.size(), active.c_str(), 
			m_logs.size(), recl.c_str());

		return str;
		//return tfm::format("Allocations %d, %s, (active %d, %s), (deallocated %d, %s)\n", 
		//	m_logs.size() + m_activeLogs.size(), total, 
		//	m_activeLogs.size(), active, 
		//	m_logs.size(), recl);
	}

	Bytes printSummary() const {
		Bytes bytes = getBytes();
		const auto total = formatBytes(bytes.total);
		const auto active = formatBytes(bytes.active);
		const auto recl = formatBytes(bytes.reclaimed);
		ark::GameLog("Allocations %d, %s, (active %d, %s), (deallocated %d, %s)\n", 
			m_logs.size() + m_activeLogs.size(), total, 
			m_activeLogs.size(), active, 
			m_logs.size(), recl);
		return bytes;
	}

	void print_all_logs() const {
		Bytes bytes = printSummary();

		ark::GameLog("Reclaimed:\n");
		for (const Log& log : m_logs) {
			ark::GameLog("pointer:%p size:%d time:%dms\n", log.pointer, log.bytes, log.time());
		}

		ark::GameLog("\nActive allocations:\n");
		for (const Log& log : m_activeLogs) {
			ark::GameLog("pointer:%p size:%d\n", log.pointer, log.bytes);
		}
	}
};

class FallBackResource : public impl_equal_std_resource {
	std::pmr::memory_resource* primary;
	std::pmr::memory_resource* secondary;
	std::deque<void*> secondaryPtrs;

public:
	FallBackResource(std::pmr::memory_resource* primary, std::pmr::memory_resource* secondary)
		: primary(primary), secondary(secondary) { }

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



