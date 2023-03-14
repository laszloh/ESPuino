#pragma once

#include <FastLED.h>
#include <iterator>
#include <cstddef>

/// Represents a set of LED objects.  Provides the [] array operator, and works like a normal array in that case.
/// This should be kept in sync with the set of functions provided by the other @ref PixelTypes as well as functions in colorutils.h.
/// @tparam PIXEL_TYPE the type of LED data referenced in the class, e.g. CRGB.
/// @note A pixel set is a window into another set of LED data, it is not its own set of LED data.
template<class PIXEL_TYPE>
class CPixelRingBuffer : public CPixelView<PIXEL_TYPE> {
public:
    const unsigned int offset;    ///< offset of the first led (always positive, dir takes care of the direction)
    PIXEL_TYPE *m_first;

public:
    /// CPixelRingBuffer copy constructor
    inline CPixelRingBuffer(const CPixelRingBuffer & other) : CPixelView<PIXEL_TYPE>(other), offset(other.offset), m_first(other.m_first) {}

    /// CPixelRingBuffer constructor for a pixel set starting at the given `PIXEL_TYPE*` and going for `_len` leds.  Note that the length
    /// can be backwards, creating a PixelSet that walks backwards over the data
    /// @param _leds pointer to the raw LED data
    /// @param _len how many LEDs in this set
    /// @param _offset the offset of the first led
    inline CPixelRingBuffer(PIXEL_TYPE *_leds, int _len, int _offset) : CPixelView<PIXEL_TYPE>(_leds, _len), offset(_offset), m_first(leds+(dir*offset)) {}

    /// CPixelRingBuffer constructor for the given set of LEDs, with start and end boundaries.  Note that start can be after
    /// end, resulting in a set that will iterate backwards
    /// @param _leds pointer to the raw LED data
    /// @param _start the start index of the LEDs for this array
    /// @param _end the end index of the LEDs for this array
    /// @param _offset the offset of the first led
    inline CPixelRingBuffer(PIXEL_TYPE *_leds, int _start, int _end, int _offset) : CPixelView<PIXEL_TYPE>(_leds, _start, _end), offset(_offset), m_first(leds+(dir*offset)) {}

    /// CPixelRingBuffer constructor from a CPixelSet
    /// @param _leds the original pixelset
    /// @param _offset the offset of the first led
    inline CPixelRingBuffer(CPixelView<PIXEL_TYPE> set, int _offset) : CPixelView<PIXEL_TYPE>(set), offset(_offset), m_first(leds+(dir*offset)) {}

    int ledOffset() const { return offset; }

    /// Do these sets point to the same thing? Note that this is different from the contents of the set being the same.
    bool operator==(const CPixelRingBuffer & rhs) const { return leds == rhs.leds && len == rhs.len && dir == rhs.dir && offset == rhs.offset; }

    /// Do these sets point to different things? Note that this is different from the contents of the set being the same.
    bool operator!=(const CPixelRingBuffer & rhs) const { return !(this == rhs); }

    /// Access a single element in this set, just like an array operator
    inline PIXEL_TYPE & operator[](int x) const {
        const int index = (x + offset) % abs(len);
        if(dir & 0x80) {
            return leds[-index];
        } else {
            return leds[index];
        }
    }

    /// Access an inclusive subset of the LEDs in this set. 
    /// @note The start point can be greater than end, which will
    /// result in a reverse ordering for many functions (useful for mirroring).
    /// @param start the first element from this set for the new subset
    /// @param end the last element for the new subset
    inline CPixelRingBuffer operator()(int start, int end, int offset) { return CPixelRingBuffer(leds, start, end, offset); }

    /// Return the reverse ordering of this set
    inline CPixelRingBuffer operator-() { return CPixelRingBuffer(leds, len - dir, 0, offset); }

    /// Return a pointer to the first element in this set (the raw pointer without offset applied)
    inline operator PIXEL_TYPE* () const { return leds; }

    /// Assign the passed in color to all elements in this set
    /// @param color the new color for the elements in the set
    inline CPixelRingBuffer & operator=(const PIXEL_TYPE & color) {
        for(iterator pixel = begin(), _end = end(); pixel != _end; ++pixel) { (*pixel) = color; }
        return *this;
    }

    /// Copy the contents of the passed-in set to our set. 
    /// @note If one set is smaller than the other, only the
    /// smallest number of items will be copied over.
    inline CPixelRingBuffer & operator=(const CPixelRingBuffer & rhs) {
        for(iterator pixel = begin(), rhspixel = rhs.begin(), _end = end(), rhs_end = rhs.end(); (pixel != _end) && (rhspixel != rhs_end); ++pixel, ++rhspixel) {
            (*pixel) = (*rhspixel);
        }
        return *this;
    }

    /// @name Color Util Functions
    /// @{

    /// Fill all of the LEDs with a rainbow of colors.
    /// @param initialhue the starting hue for the rainbow
    /// @param deltahue how many hue values to advance for each LED
    /// @see ::fill_rainbow(struct CRGB*, int, uint8_t, uint8_t)
    inline CPixelRingBuffer & fill_rainbow(uint8_t initialhue, uint8_t deltahue=5) {
        CHSV hsv;
        hsv.hue = initialhue;
        hsv.val = 255;
        hsv.sat = 240;
        for( int i = 0; i < abs(len); ++i) {
            operator[](i) = hsv;
            hsv.hue += deltahue;
        }
        return *this;
    }

    void int_fill_gradient(uint16_t startpos, CHSV startcolor,
                    uint16_t endpos,   CHSV endcolor,
                    TGradientDirectionCode directionCode  = SHORTEST_HUES )
    {
        // if the points are in the wrong order, straighten them
        if( endpos < startpos ) {
            uint16_t t = endpos;
            CHSV tc = endcolor;
            endcolor = startcolor;
            endpos = startpos;
            startpos = t;
            startcolor = tc;
        }

        // If we're fading toward black (val=0) or white (sat=0),
        // then set the endhue to the starthue.
        // This lets us ramp smoothly to black or white, regardless
        // of what 'hue' was set in the endcolor (since it doesn't matter)
        if( endcolor.value == 0 || endcolor.saturation == 0) {
            endcolor.hue = startcolor.hue;
        }

        // Similarly, if we're fading in from black (val=0) or white (sat=0)
        // then set the starthue to the endhue.
        // This lets us ramp smoothly up from black or white, regardless
        // of what 'hue' was set in the startcolor (since it doesn't matter)
        if( startcolor.value == 0 || startcolor.saturation == 0) {
            startcolor.hue = endcolor.hue;
        }

        saccum87 huedistance87;
        saccum87 satdistance87;
        saccum87 valdistance87;

        satdistance87 = (endcolor.sat - startcolor.sat) << 7;
        valdistance87 = (endcolor.val - startcolor.val) << 7;

        uint8_t huedelta8 = endcolor.hue - startcolor.hue;

        if( directionCode == SHORTEST_HUES ) {
            directionCode = FORWARD_HUES;
            if( huedelta8 > 127) {
                directionCode = BACKWARD_HUES;
            }
        }

        if( directionCode == LONGEST_HUES ) {
            directionCode = FORWARD_HUES;
            if( huedelta8 < 128) {
                directionCode = BACKWARD_HUES;
            }
        }

        if( directionCode == FORWARD_HUES) {
            huedistance87 = huedelta8 << 7;
        }
        else /* directionCode == BACKWARD_HUES */
        {
            huedistance87 = (uint8_t)(256 - huedelta8) << 7;
            huedistance87 = -huedistance87;
        }

        uint16_t pixeldistance = endpos - startpos;
        int16_t divisor = pixeldistance ? pixeldistance : 1;

        saccum87 huedelta87 = huedistance87 / divisor;
        saccum87 satdelta87 = satdistance87 / divisor;
        saccum87 valdelta87 = valdistance87 / divisor;

        huedelta87 *= 2;
        satdelta87 *= 2;
        valdelta87 *= 2;

        accum88 hue88 = startcolor.hue << 8;
        accum88 sat88 = startcolor.sat << 8;
        accum88 val88 = startcolor.val << 8;
        for( uint16_t i = startpos; i <= endpos; ++i) {
            operator[](i) = CHSV( hue88 >> 8, sat88 >> 8, val88 >> 8);
            hue88 += huedelta87;
            sat88 += satdelta87;
            val88 += valdelta87;
        }
    }

    /// Fill all of the LEDs with a smooth HSV gradient between two HSV colors. 
    /// @param startcolor the starting color in the gradient
    /// @param endcolor the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelRingBuffer & fill_gradient(const CHSV & startcolor, const CHSV & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
        uint16_t last = this->size() - 1;
        int_fill_gradient(0, startcolor, last, endcolor, directionCode);
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between three HSV colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the middle color for the gradient
    /// @param c3 the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelRingBuffer & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV &  c3, TGradientDirectionCode directionCode = SHORTEST_HUES) {
        uint16_t half = (this->size() / 2);
        uint16_t last = this->size() - 1;
        int_fill_gradient(   0, c1, half, c2, directionCode);
        int_fill_gradient(half, c2, last, c3, directionCode);
        return *this;
    }

    /// Fill all of the LEDs with a smooth HSV gradient between four HSV colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the first middle color for the gradient
    /// @param c3 the second middle color for the gradient
    /// @param c4 the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient(T*, uint16_t, const CHSV&, const CHSV&, const CHSV&, const CHSV&, TGradientDirectionCode)
    inline CPixelRingBuffer & fill_gradient(const CHSV & c1, const CHSV & c2, const CHSV & c3, const CHSV & c4, TGradientDirectionCode directionCode = SHORTEST_HUES) {
        uint16_t onethird = (this->size() / 3);
        uint16_t twothirds = ((this->size() * 2) / 3);
        uint16_t last = this->size() - 1;
        int_fill_gradient(        0, c1,  onethird, c2, directionCode);
        int_fill_gradient( onethird, c2, twothirds, c3, directionCode);
        int_fill_gradient(twothirds, c3,      last, c4, directionCode);
        return *this;
    }
    
    void int_fill_gradient_RGB(uint16_t startpos, PIXEL_TYPE startcolor, uint16_t endpos, PIXEL_TYPE endcolor)
    {
        // if the points are in the wrong order, straighten them
        if( endpos < startpos ) {
            uint16_t t = endpos;
            PIXEL_TYPE tc = endcolor;
            endcolor = startcolor;
            endpos = startpos;
            startpos = t;
            startcolor = tc;
        }

        saccum87 rdistance87;
        saccum87 gdistance87;
        saccum87 bdistance87;

        rdistance87 = (endcolor.r - startcolor.r) << 7;
        gdistance87 = (endcolor.g - startcolor.g) << 7;
        bdistance87 = (endcolor.b - startcolor.b) << 7;

        uint16_t pixeldistance = endpos - startpos;
        int16_t divisor = pixeldistance ? pixeldistance : 1;

        saccum87 rdelta87 = rdistance87 / divisor;
        saccum87 gdelta87 = gdistance87 / divisor;
        saccum87 bdelta87 = bdistance87 / divisor;

        rdelta87 *= 2;
        gdelta87 *= 2;
        bdelta87 *= 2;

        accum88 r88 = startcolor.r << 8;
        accum88 g88 = startcolor.g << 8;
        accum88 b88 = startcolor.b << 8;
        for( uint16_t i = startpos; i <= endpos; ++i) {
            operator[](i) = PIXEL_TYPE( r88 >> 8, g88 >> 8, b88 >> 8);
            r88 += rdelta87;
            g88 += gdelta87;
            b88 += bdelta87;
        }
    }

    /// Fill all of the LEDs with a smooth RGB gradient between two RGB colors. 
    /// @param startcolor the starting color in the gradient
    /// @param endcolor the end color for the gradient
    /// @param directionCode the direction to travel around the color wheel
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&)
    inline CPixelRingBuffer & fill_gradient_RGB(const PIXEL_TYPE & startcolor, const PIXEL_TYPE & endcolor, TGradientDirectionCode directionCode  = SHORTEST_HUES) {
        uint16_t last = this->size() - 1;
        int_fill_gradient_RGB(0, startcolor, last, endcolor);
        return *this;
    }

    /// Fill all of the LEDs with a smooth RGB gradient between three RGB colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the middle color for the gradient
    /// @param c3 the end color for the gradient
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&, const CRGB&)
    inline CPixelRingBuffer & fill_gradient_RGB(const PIXEL_TYPE & c1, const PIXEL_TYPE & c2, const PIXEL_TYPE &  c3) {
        uint16_t half = (this->size() / 2);
        uint16_t last = this->size() - 1;
        int_fill_gradient_RGB(    0, c1, half, c2);
        int_fill_gradient_RGB( half, c2, last, c3);
        return *this;
    }

    /// Fill all of the LEDs with a smooth RGB gradient between four RGB colors. 
    /// @param c1 the starting color in the gradient
    /// @param c2 the first middle color for the gradient
    /// @param c3 the second middle color for the gradient
    /// @param c4 the end color for the gradient
    /// @see ::fill_gradient_RGB(CRGB*, uint16_t, const CRGB&, const CRGB&, const CRGB&, const CRGB&)
    inline CPixelRingBuffer & fill_gradient_RGB(const PIXEL_TYPE & c1, const PIXEL_TYPE & c2, const PIXEL_TYPE & c3, const PIXEL_TYPE & c4) {
        uint16_t onethird = (this->size() / 3);
        uint16_t twothirds = ((this->size() * 2) / 3);
        uint16_t last = this->size() - 1;
        int_fill_gradient_RGB(         0, c1, onethird, c2);
        int_fill_gradient_RGB( onethird, c2, twothirds, c3);
        int_fill_gradient_RGB(twothirds, c3, last, c4);
        return *this;
    }

    /// @} Color Util Functions

    /// @name Iterator
    /// @{

    /// Iterator helper class for CPixelRingBuffer
    /// @tparam the type of the LED array data
    /// @todo Make this a fully specified/proper iterator
    template <class T>
    class pixelbuf_iterator {
        using iterator_category = std::forward_iterator_tag;    // could extend it to bidirectional_iterator_tag
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = value_type*;
        using reference         = value_type&;
        
        const CPixelRingBuffer<T> *m_buff;
        pointer m_it;           ///< pointer to LED array

        void increment() {
            m_it += m_buff->dir;
            if(m_it == m_buff->end_pos) {
                m_it = m_buff->leds;
            }
        }

    public:
        /// Default constructor
        __attribute__((always_inline)) inline pixelbuf_iterator() : m_buff(0), m_it(0) {}
        
        /// Copy constructor
        __attribute__((always_inline)) inline pixelbuf_iterator(const pixelbuf_iterator &rhs) : m_buff(rhs.m_buff), m_it(rhs.m_it) {}
    
        /// Internal constructor
        __attribute__((always_inline)) inline pixelbuf_iterator(const CPixelRingBuffer<T> *cb, pointer p) : m_buff(cb), m_it(p) {}
    
        /// assign operator
        __attribute__((always_inline)) inline pixelbuf_iterator& operator=(const pixelbuf_iterator<T>&) = default;
    
        /// random access operator
        __attribute__((always_inline)) inline reference operator*() const { return *m_it; }
        
        __attribute__((always_inline)) inline pointer operator->() const { return &(operator*()); }
            
        /// increment operator (postfix)
        __attribute__((always_inline)) inline pixelbuf_iterator operator++(int) {
            pixelbuf_iterator<T> tmp = *this;
            ++*this;
            return tmp;
        }
        
        /// increment operator (prefix)
        __attribute__((always_inline)) inline pixelbuf_iterator operator++() {
            increment();
            if(m_it == m_buff->m_first) {
                m_it = 0;
            }
            return *this;
        }
        
        /// Equality comparison
        __attribute__((always_inline)) inline bool operator==(const pixelbuf_iterator<T> &it) const { return m_it == it.m_it; }
        
        /// Inequality comparison
        __attribute__((always_inline)) inline bool operator!=(const pixelbuf_iterator<T> &it) const { return m_it != it.m_it; }

    };

    typedef pixelbuf_iterator<PIXEL_TYPE> iterator;              ///< Iterator helper type for this class
    typedef pixelbuf_iterator<const PIXEL_TYPE> const_iterator;  ///< Const iterator helper type for this class

    iterator begin() { return iterator(this, m_first); }   ///< Makes an iterator instance for the start of the LED set
    iterator end() { return iterator(this, 0); }  ///< Makes an iterator instance for the end of the LED set

    iterator begin() const { return iterator(this, m_first); }   ///< Makes an iterator instance for the start of the LED set, const qualified
    iterator end() const { return iterator(this, 0); }  ///< Makes an iterator instance for the end of the LED set, const qualified

    const_iterator cbegin() const { return const_iterator(this, m_first); }   ///< Makes a const iterator instance for the start of the LED set, const qualified
    const_iterator cend() const { return const_iterator(this, 0); }  ///< Makes a const iterator instance for the end of the LED set, const qualified

    /// @} Iterator

private:

    using CPixelView<PIXEL_TYPE>::dir;
    using CPixelView<PIXEL_TYPE>::len;
    using CPixelView<PIXEL_TYPE>::leds;
    using CPixelView<PIXEL_TYPE>::end_pos;
};