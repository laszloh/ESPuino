#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <stdlib.h>
#include <string>
#include <vector>

// Custom allocator for PSRAM if available
template <typename T>
class PSRAMAllocator {
public:
	using value_type = T;

	PSRAMAllocator() = default;
	template <typename U>
	PSRAMAllocator(const PSRAMAllocator<U> &) { }

	T *allocate(size_t n) {
		if (psramFound()) {
			if (T *ptr = static_cast<T *>(ps_malloc(n * sizeof(T)))) {
				return ptr;
			}
		}
		if (T *ptr = static_cast<T *>(malloc(n * sizeof(T)))) {
			return ptr;
		}
		throw std::bad_alloc();
	}

	void deallocate(T *ptr, size_t) {
		free(ptr);
	}
};

template <typename T, typename U>
bool operator==(const PSRAMAllocator<T> &, const PSRAMAllocator<U> &) {
	return true;
}
template <typename T, typename U>
bool operator!=(const PSRAMAllocator<T> &, const PSRAMAllocator<U> &) {
	return false;
}

using PlaylistString = std::basic_string<char, std::char_traits<char>, PSRAMAllocator<char>>;

// Playlist allocates its own control block in PSRAM via operator new,
// and its element buffer in PSRAM via PSRAMAllocator.
class Playlist : public std::vector<PlaylistString, PSRAMAllocator<PlaylistString>> {
public:
	using std::vector<PlaylistString, PSRAMAllocator<PlaylistString>>::vector;

	static void *operator new(std::size_t size) {
		if (psramFound()) {
			if (void *p = ps_malloc(size)) {
				return p;
			}
		}
		if (void *p = malloc(size)) {
			return p;
		}
		throw std::bad_alloc();
	}

	static void operator delete(void *p) noexcept {
		free(p);
	}
};
