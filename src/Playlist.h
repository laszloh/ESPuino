#pragma once

#include "cpp.h"

#include <WString.h>
#include <algorithm>
#include <string.h>
#include <string>

using sortFunc = std::function<int(const void *, const void *)>;

class Playlist {
public:
	Playlist() { }
	virtual ~Playlist() { }

	virtual size_t size() const { return 0; }

	virtual bool isValid() const { return false; }
	explicit operator bool() const { return isValid(); }

	virtual const String getAbsolutePath(size_t idx) const { return String {}; }

	virtual const String getFilename(size_t idx) const { return String {}; }

	static int alphabeticSort(const void *x, const void *y) {
		const char *a = static_cast<const char *>(x);
		const char *b = static_cast<const char *>(y);

		return strcmp(a, b);
	}

	virtual void sort(sortFunc func = alphabeticSort) { }
	template <typename _Compare>
	void sort(_Compare c) {
		sort(c);
	}

	virtual void randomize() { }

	// Check if file-type is correct
	static bool fileValid(const String _fileItem) {
		constexpr size_t maxExtLen = strlen(*std::max_element(audioFileSufix.begin(), audioFileSufix.end(), [](const char *a, const char *b) {
			return strlen(a) < strlen(b);
		}));

		if (!_fileItem) {
			return false;
		}

		// check for http address
		if (_fileItem.startsWith("http://") || _fileItem.startsWith("https://")) {
			return true;
		}

		// Ignore hidden files starting with a '.'
		//    lastIndex is -1 if '/' is not found --> first index will be 0
		int fileNameIndex = _fileItem.lastIndexOf('/') + 1;
		if (_fileItem[fileNameIndex] == '.') {
			return false;
		}

		String extBuf;
		const size_t extStart = _fileItem.lastIndexOf('.');
		const size_t extLen = _fileItem.length() - extStart;
		if (extLen > maxExtLen) {
			// we either did not find a . or extension was too long
			return false;
		}
		extBuf = _fileItem.substring(extStart);
		extBuf.toLowerCase();

		for (const auto e : audioFileSufix) {
			if (extBuf.equals(e)) {
				return true;
			}
		}
		return false;
	}

protected:
	template <typename T>
	class PsramAllocator : public std::allocator<T> {
	public:
		using value_type = T;
		using pointer = T *;
		using const_pointer = const T *;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;

	public:
		template <typename U>
		struct rebind {
			typedef PsramAllocator<U> other;
		};

		inline explicit PsramAllocator()
			: std::allocator<T>() { }
		inline PsramAllocator(PsramAllocator const &a)
			: std::allocator<T>(a) { }
		template <typename U>
		inline explicit PsramAllocator(PsramAllocator<U> const &a)
			: std::allocator<T>(a) { }

		inline ~PsramAllocator() { }

		// memory allocation
		inline pointer allocate(size_type cnt) {
			T *ptr = nullptr;
			if (psramFound()) {
				ptr = (T *) ps_malloc(cnt * sizeof(T));
			} else {
				ptr = (T *) malloc(cnt * sizeof(T));
			}
			return ptr;
		}

		inline void deallocate(pointer p, size_type cnt) {
			free(p);
		}

		//   size
		inline size_type max_size() const {
			return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// construction/destruction
		inline void construct(pointer p, const T &t) {
			new (p) T(t);
		}

		inline void destroy(pointer p) {
			p->~T();
		}

		inline bool operator==(PsramAllocator const &a) { return this == &a; }
		inline bool operator!=(PsramAllocator const &a) { return !operator==(a); }
	};

	using pstring = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

	virtual void destroy() { }

	// clang-format off
	static constexpr auto audioFileSufix = std::to_array<const char*>({
		".mp3",
		".aac",
		".m4a",
		".wav",
		".flac",
		".aac",
		// playlists
		".m3u",
		".m3u8",
		".pls",
		".asx"
	});
	// clang-format on
};
