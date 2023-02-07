#pragma once

#include <pixelset.h>

template<class PIXEL_TYPE>
class CPixelViewOffset : public CPixelView<PIXEL_TYPE>{
public:
    size_t offset; ///< offset of the "virtual" 0th led to the start position of the array. Always positive.

public:
    /// CPixelViewOffset copy constructor
    inline CPixelViewOffset(const CPixelViewOffset & other) : CPixelView<PIXEL_TYPE>(other), offset(other.offset) {}

    /// PixelSet constructor for a pixel set starting at the given `PIXEL_TYPE*` and going for `_len` leds.  Note that the length
    /// can be backwards, creating a PixelSet that walks backwards over the data
    /// @param _leds pointer to the raw LED data
    /// @param _len how many LEDs in this set
    /// @param _offset the offset between the real and virtual first led
    inline CPixelViewOffset(PIXEL_TYPE *_leds, int _len, size_t _offset) : CPixelView<PIXEL_TYPE>(_leds, _len), offset(_offset) {}

    /// PixelSet constructor for the given set of LEDs, with start and end boundaries.  Note that start can be after
    /// end, resulting in a set that will iterate backwards
    /// @param _leds pointer to the raw LED data
    /// @param _start the start index of the LEDs for this array
    /// @param _end the end index of the LEDs for this array
    inline CPixelViewOffset(PIXEL_TYPE *_leds, int _start, int _end, size_t _offset) : CPixelView<PIXEL_TYPE>(_leds, _start, _end), offset(_offset) {}

    /// Access a single element in this set, just like an array operator
    inline PIXEL_TYPE & operator[](int x) const {
        // calculate the index to access (like a ring buffer)
        if(dir & 0x80) {
            const size_t idx = (-len - 1 + (x + offset)) % (-len);
            return leds[-idx];
        } else {
            const size_t idx = (x + offset) % len;
            return leds[idx];
        }
    }

    /// Access an inclusive subset of the LEDs in this set. 
    /// @note The start point can be greater than end, which will
    /// result in a reverse ordering for many functions (useful for mirroring).
    /// @param start the first element from this set for the new subset
    /// @param end the last element for the new subset
    /// @param offset the offset between the real and the virtual first eleent
    inline CPixelViewOffset operator()(int start, int end, int offset) { return CPixelView(this->leds, start, end, offset); }

    // Access an inclusive subset of the LEDs in this set, starting from the first.
    // @param end the last element for the new subset
    // @todo Not sure i want this? inline CPixelView operator()(int end) { return CPixelView(leds, 0, end); }

    /// Return the reverse ordering of this set
    inline CPixelViewOffset operator-() { return CPixelViewOffset(this->leds, this->len - this->dir, 0, offset); }

    /// Assign the passed in color to all elements in this set
    /// @param color the new color for the elements in the set
    inline CPixelViewOffset & operator=(const PIXEL_TYPE & color) {
        for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) = color; }
        return *this;
    }

    inline CPixelViewOffset & operator=(const CPixelViewOffset & rhs) {
        for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) {
            (*pixel) = (*rhspixel);
        }
        return *this;
    }


        /// @name Iterator
    /// @{

    /// Iterator helper class for CPixelView
    /// @tparam the type of the LED array data
    /// @todo Make this a fully specified/proper iterator
    template <class T>
    class pixelset_iterator_base {
        T * leds;          ///< pointer to LED array
        const int8_t dir;  ///< direction of LED array, for incrementing the pointer

    public:
        /// Copy constructor
        __attribute__((always_inline)) inline pixelset_iterator_base(const pixelset_iterator_base & rhs) : leds(rhs.leds), dir(rhs.dir) {}

        /// Base constructor
        /// @tparam the type of the LED array data
        /// @param _leds pointer to LED array
        /// @param _dir direction of LED array
        __attribute__((always_inline)) inline pixelset_iterator_base(T * _leds, const char _dir) : leds(_leds), dir(_dir) {}

        __attribute__((always_inline)) inline pixelset_iterator_base& operator++() { leds += dir; return *this; }  ///< Increment LED pointer in data direction
        __attribute__((always_inline)) inline pixelset_iterator_base operator++(int) { pixelset_iterator_base tmp(*this); leds += dir; return tmp; }  ///< @copydoc operator++()

        __attribute__((always_inline)) inline bool operator==(pixelset_iterator_base & other) const { return leds == other.leds; /* && set==other.set; */ }    ///< Check if iterator is at the same position
        __attribute__((always_inline)) inline bool operator!=(pixelset_iterator_base & other) const { return leds != other.leds; /* || set != other.set; */ }  ///< Check if iterator is not at the same position

        __attribute__((always_inline)) inline PIXEL_TYPE& operator*() const { return *leds; }  ///< Dereference operator, to get underlying pointer to the LEDs
    };
    
    typedef pixelset_iterator_base<PIXEL_TYPE> iterator;              ///< Iterator helper type for this class
    typedef pixelset_iterator_base<const PIXEL_TYPE> const_iterator;  ///< Const iterator helper type for this class

    iterator begin() { return iterator(leds, dir); }   ///< Makes an iterator instance for the start of the LED set
    iterator end() { return iterator(end_pos, dir); }  ///< Makes an iterator instance for the end of the LED set

    iterator begin() const { return iterator(leds, dir); }   ///< Makes an iterator instance for the start of the LED set, const qualified
    iterator end() const { return iterator(end_pos, dir); }  ///< Makes an iterator instance for the end of the LED set, const qualified

    const_iterator cbegin() const { return const_iterator(leds, dir); }   ///< Makes a const iterator instance for the start of the LED set, const qualified
    const_iterator cend() const { return const_iterator(end_pos, dir); }  ///< Makes a const iterator instance for the end of the LED set, const qualified

private:
    using CPixelView<PIXEL_TYPE>::dir;
    using CPixelView<PIXEL_TYPE>::leds;
    using CPixelView<PIXEL_TYPE>::len;
    using CPixelView<PIXEL_TYPE>::end_pos;
};
