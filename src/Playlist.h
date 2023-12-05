#pragma once

#include <stdlib.h>
#include <vector>

template <typename T>
class PsramAllocator {
public:
	using value_type = T;
	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;

	PsramAllocator() noexcept = default;
	PsramAllocator(PsramAllocator const &) noexcept = default;
	~PsramAllocator() = default;

	template <typename U>
	constexpr PsramAllocator(const PsramAllocator<U> const &) noexcept { }

	// memory allocation
	[[nodiscard]] T *allocate(std::size_t cnt) {
		return (T *) heap_caps_malloc_prefer(cnt * sizeof(T), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	}

	void deallocate(T *p, std::size_t cnt) {
		free(p);
	}

	constexpr bool operator==(PsramAllocator const &a) { return true; }
	constexpr bool operator!=(PsramAllocator const &a) { return false; }
};

using pstring = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;
using Playlist = std::vector<pstring>;
