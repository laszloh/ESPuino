#pragma once

#include <Arduino.h>

#include <stdlib.h>
#include <string>
#include <vector>

template <typename T>
struct PsramAllocator {
	using value_type = T;

	PsramAllocator() = default;

	template <typename U>
	constexpr PsramAllocator(const PsramAllocator<U> &) noexcept { }

	T *allocate(std::size_t n) {
		static const bool hasPsram = psramFound();
		void *ptr = hasPsram ? ps_malloc(n * sizeof(T)) : malloc(n * sizeof(T));
		return static_cast<T *>(ptr);
	}

	void deallocate(T *ptr, std::size_t) noexcept {
		free(ptr);
	}

	template <typename U>
	bool operator==(const PsramAllocator<U> &) const noexcept { return true; }

	template <typename U>
	bool operator!=(const PsramAllocator<U> &) const noexcept { return false; }
};

using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;
using Playlist = std::vector<PsramString, PsramAllocator<PsramString>>;
