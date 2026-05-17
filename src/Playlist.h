#pragma once

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
			T *ptr = static_cast<T *>(ps_malloc(n * sizeof(T)));
			if (ptr) {
				return ptr;
			}
		}
		return static_cast<T *>(malloc(n * sizeof(T)));
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
using Playlist = std::vector<PlaylistString, PSRAMAllocator<PlaylistString>>;

// Allocate Playlist in PSRAM if available
inline Playlist *allocatePlaylist() {
	if (psramFound()) {
		void *mem = ps_malloc(sizeof(Playlist));
		if (mem) {
			return new (mem) Playlist();
		}
	}
	return new Playlist();
}

// Release previously allocated memory
inline void freePlaylist(Playlist *(&playlist)) {
	if (playlist == nullptr) {
		return;
	}
	playlist->~Playlist(); // Call destructor explicitly
	free(playlist); // Use free instead of delete since it might be ps_malloc'd
	playlist = nullptr;
}
