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
	constexpr PsramAllocator(const PsramAllocator<U> &) noexcept { }

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

class Playlist {
public:
	Playlist() = default;
	Playlist(size_t reserve)
		: entries(std::vector<pstring>(reserve)) { }
	virtual ~Playlist() = default;

	// override bool operator so we can write if(playlist)
	explicit operator bool() const { return isValid(); }

	// return true if the playlist is in a valid state (so we have entries and we want to play them)
	bool isValid() const { return (entries.size() > 0) && (currentTrack < entries.size()); }

	// return a playlist entry. Function throws std::out_of_range if index >= size
	const pstring &getTrackPath(size_t index) const { return entries.at(index); }

	// return the current playlist entry. Function throws std::out_of_range if currentTrack >= size
	const pstring &getCurrentTrackPath() const { return entries.at(currentTrack); }

	uint16_t getCurrentTrackNumber() const { return currentTrack; }
	bool setCurrentTrackNumber(size_t track) { return updateCurrentTrack(track); }
	bool incrementTrackNumber() { return updateCurrentTrack(currentTrack + 1); }
	bool decrementTrackNumber() { return updateCurrentTrack(currentTrack - 1); }

	bool loopTrack {false};

	bool loopPlaylist {false};

	size_t size() const { return entries.size(); }

	void push_back(const pstring &path) { entries.push_back(path); }

	// disable copying
	Playlist(const Playlist &) = delete;
	Playlist &operator=(const Playlist &) = delete;

protected:
	// Enable move operators
	Playlist(Playlist &&) = default;
	Playlist &operator=(Playlist &&) = default;

	bool updateCurrentTrack(int32_t nextTrack) {
		if (loopTrack) {
			// we are looping a single track, so nothing to do here
			return true;
		}
		// nexttrack is always
		if (loopPlaylist) {
			if (nextTrack > entries.size()) {
				// we overshoot the entries size --> roll over to zero
				nextTrack %= entries.size();
			} else if (nextTrack < 0) {
				// we underhsoot the playlist --> roll over to size()
				nextTrack = entries.size() - std::abs(nextTrack);
			}
		}
		if (nextTrack >= 0 && nextTrack < entries.size()) {
			// next track has a valid value (so we did not reach the end of the playlist)
			currentTrack = nextTrack;
			return true;
		}
		return false;
	}

	std::vector<pstring> entries {std::vector<pstring>()};
	uint16_t currentTrack {std::numeric_limits<uint16_t>::max()};
};
