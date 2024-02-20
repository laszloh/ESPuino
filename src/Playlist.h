#pragma once

#include "MediaItem.hpp"

#include <stdlib.h>

/// @brief Interface class representing a playlist
class Playlist {
public:
	/**
	 * @brief signature for the compare function
	 * @param a First element for the compare
	 * @param a Second element for the compare
	 * @return true if the expression a<b allpies (so if the first element is *less* than the second)
	 */
	using CompareFunc = std::function<bool(const MediaItem &a, const MediaItem &b)>;

	Playlist() = default;
	virtual ~Playlist() = default;

	/**
	 * @brief get the status of the playlist
	 * @return true If the playlist has at least 1 playable entry
	 * @return false If the playlist is invalid
	 */
	virtual bool isValid() const = 0;

	/**
	 * @brief Allow explicit bool conversions, like when calling `if(playlist)`
	 * @see isValid()
	 * @return A bool conversion representing the status of the playlist
	 */
	explicit operator bool() const { return isValid(); }

	/**
	 * @brief Get the number of entries in the playlist
	 * @return size_t The number of MediaItem elemenets in the underlying container
	 */
	virtual size_t size() const = 0;

	/**
	 * @brief Get the element at index
	 * @param idx The queried index
	 * @return MediaItem the data at the index
	 */
	virtual MediaItem &at(size_t idx) = 0;

	/**
	 * @brief Add an item at the end of the container
	 * @param item The new item ot be added
	 * @return const MediaItem the data at the index
	 */
	virtual void addMediaItem(const MediaItem &&item) = 0;

	/**
	 * @brief Add an item at the end of the container
	 * @param item The new item ot be added
	 * @param idx after which entry it'll be added
	 * @return const MediaItem the data at the index
	 */
	virtual void addMediaItem(const MediaItem &&item, int idx) = 0;

	/**
	 * @brief Sort the underlying container according to the supplied sort functions
	 * @param comp The compare function to use, defaults to strcmp between the two uri objects
	 */
	virtual void sort(CompareFunc comp = [](const MediaItem &a, const MediaItem &b) -> bool { return a.uri < b.uri; }) { }

	/**
	 * @brief Randomize the underlying entries
	 */
	virtual void shuffle() { }

	/**
	 * @brief Array opertor override for item access
	 * @param idx the queried index
	 * @return const MediaItem& Reference to the MediaItem at the index
	 */
	const MediaItem &operator[](size_t idx) const { return at(idx); };

	///@brief Iterator class to access playlist items
	class Iterator {
	public:
		// define what we can do
		using iterator_category = std::random_access_iterator_tag; //< support random access iterators
		using difference_type = std::ptrdiff_t;
		using value_type = MediaItem;
		using pointer = value_type *;
		using reference = value_type &;

		// Lifecycle
		Iterator() = default;
		Iterator(Playlist playlist, size_t idx)
			: m_playlist(playlist)
			, m_idx(idx) { }

		// Operators: Access
		inline Type &operator*() { return m_playlist->at(m_idx); }
		inline Type *operator->() { return &m_playlist->at(m_idx); }
		inline Type &operator[](const int &rhs) { return m_playlist->at(rhs); }

		// Operators: arithmetic
		// clang-format off
		inline Iterator& operator++() {++m_idx; return *this;}
        inline Iterator& operator--() {--m_idx; return *this;}
        inline Iterator& operator++(int) {Iterator tmp(*this); ++m_idx; return tmp;}
        inline Iterator& operator--(int) {Iterator tmp(*this); --m_idx; return tmp;}
        inline Iterator operator+(const Iterator& rhs) {return Iterator(m_playlist, m_idx + rhs.ptr);}
        inline Iterator operator-(const Iterator& rhs) {return Iterator(m_playlist, m_idx - rhs.ptr);}
        inline Iterator operator+(const int& rhs) {return Iterator(m_playlist, m_idx + rhs);}
        inline Iterator operator-(const int& rhs) {return Iterator(m_playlist, m_idx - rhs);}
        friend inline Iterator operator+(const int& lhs, const Iterator& rhs) {return Iterator(rhs.m_playlist, lhs + rhs.m_idx);}
        friend inline Iterator operator-(const int& lhs, const Iterator& rhs) {return Iterator(rhs.m_playlist, lhs - rhs.m_idx);}
		// clang-format on

		// Operators: arithmetic
		inline bool operator==(const Iterator &rhs) { return m_idx == rhs.m_idx; }
		inline bool operator!=(const Iterator &rhs) { return m_idx != rhs.m_idx; }
		inline bool operator>(const Iterator &rhs) { return m_idx > rhs.m_idx; }
		inline bool operator<(const Iterator &rhs) { return m_idx < rhs.m_idx; }
		inline bool operator>=(const Iterator &rhs) { return m_idx >= rhs.m_idx; }
		inline bool operator<=(const Iterator &rhs) { return m_idx <= rhs.m_idx; }

	protected:
		Playlist *m_playlist {nullptr};
		size_t m_idx {0};
	};

	Iterator begin() const { return Iterator(this, 0); }
	Iterator end() const { return Iterator(this, size()); }
};
