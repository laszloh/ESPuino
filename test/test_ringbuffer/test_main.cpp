#include <Arduino.h>
#include <unity.h>
#include <array>

#include <FastLED.h>
#include "pixelringbuffer.h"

CRGBArray<32> leds;

// set stuff up here, this function is before a test function
void setUp() {
    // prefill led array
    uint8_t r = 0;
    for(auto &px : leds) {
        px.setRGB(r, 128, 128);
        r += 10;
    }
}

void tearDown() {
}

void test_basic_buffer() {
    CPixelRingBuffer<CRGB> dut(leds, 0, 15, 0);

    TEST_ASSERT_EQUAL_MESSAGE(16, dut.size(), "RingBuffer returned incorrect size");
    TEST_ASSERT_FALSE_MESSAGE(dut.reversed(), "RingBuffer is reversed");

    // read underlying array 
    for(uint8_t i=0;i<dut.size();i++) {
        TEST_ASSERT_EQUAL_MESSAGE(i * 10, dut[i].r, "RingBuffer operator[] failed");
    }

    // write and read underlying array
    for(uint8_t i=0, g=0;i<dut.size();i++, g+=10) {
        TEST_ASSERT_EQUAL_MESSAGE(i*10, dut[i].r, "LED array was modified");
        dut[i].g = g;
        TEST_ASSERT_EQUAL_MESSAGE(g, dut[i].g, "Write did not propagate to underlying array");
    }
}

void test_basic_reverse() {
    CPixelRingBuffer<CRGB> dut(leds(15, 0), 0);

    TEST_ASSERT_EQUAL_MESSAGE(16, dut.size(), "RingBuffer returned incorrect size");
    TEST_ASSERT_TRUE_MESSAGE(dut.reversed(), "RingBuffer is not reversed");

    // read underlying array 
    for(uint8_t i=0,r=150;i<dut.size();i++,r-=10) {
        String text = "RingBuffer operator[] failed at index=" + String(i);
        TEST_ASSERT_EQUAL_MESSAGE(r, dut[i].r, text.c_str());
    }

    // write and read underlying array
    for(uint8_t i=0, r=150;i<dut.size();i++, r-=10) {
        String text = "RingBuffer operator[] failed at index=" + String(i);
        dut[i].g = i;
        TEST_ASSERT_EQUAL_MESSAGE(i, dut[i].g, text.c_str());
    }

    // access leds array and confirm, we wrote correctly into dut
    for(uint8_t i=0, g=15;i<dut.size();i++, g--) {
        TEST_ASSERT_EQUAL_MESSAGE(g, leds[i].g, "RingBuffer operator[] access array in incorrect way");
    }
}

void test_basic_offset() {
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds(0, 15), offset);

    TEST_ASSERT_EQUAL_MESSAGE(16, dut.size(), "RingBuffer returned incorrect size");
    TEST_ASSERT_FALSE_MESSAGE(dut.reversed(), "RingBuffer is reversed");

    // read underlying array
    // iterators not yet support offset. So use plain old for-loop
    for(uint8_t i=0;i<dut.size();i++) {
        String text = "RingBuffer operator[] failed at index=" + String(i);
        uint8_t r = ((i+offset) % dut.size()) * 10;
        TEST_ASSERT_EQUAL_MESSAGE(r, dut[i].r, text.c_str());
    }
}

int mod(int x, int m) {
    return (x%m + m)%m;
}

void test_complex_usage() {
    // here we both test reverse and offset array access
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds(15,0), offset);

    TEST_ASSERT_EQUAL_MESSAGE(16, dut.size(), "RingBuffer returned incorrect size");
    TEST_ASSERT_TRUE_MESSAGE(dut.reversed(), "RingBuffer is not reversed");

    // check access to underlying array
    for(uint8_t i=0;i<dut.size();i++) {
        String text = "RingBuffer operator[] failed at index=" + String(i);
        uint8_t r = mod(dut.size() - 1 - (i + offset), dut.size()) * 10;
        TEST_ASSERT_EQUAL_MESSAGE(r, dut[i].r, text.c_str());

        dut[i].g = i;
    }

    // check original array
    for(uint8_t i=0;i<dut.size();i++) {
        String text = "RingBuffer operator[] failed at index=" + String(i);
        uint8_t g = mod(dut.size() - 1 - (i + offset), dut.size());
        TEST_ASSERT_EQUAL_MESSAGE(g, leds[i].g, text.c_str());
    }
}

void verifyCheckAgainstDut(CRGBSet &check, CPixelRingBuffer<CRGB> &dut) {
    for(uint8_t i=0;i<check.size();i++) {
        String text = "Wrong color in RingBuffer at index=" + String(i) + " (should: 0x" + String(uint32_t(check[i]), 16) + " is: 0x" + String(uint32_t(dut[i]), 16) + ")";
        TEST_ASSERT_TRUE_MESSAGE(check[i] == dut[i], text.c_str());
    }
}

void test_fill_solid(){
    constexpr CRGB::HTMLColorCode baseColor = CRGB::RoyalBlue;
    constexpr CRGB::HTMLColorCode dutColor = CRGB::Aqua;
    
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds, 16, offset);

    leds.fill_solid(baseColor);
    dut.fill_solid(dutColor);

    // test underlying array
    for(uint8_t i=0;i<leds.size();i++) {
        CRGB test = (i<dut.size()) ? dutColor : baseColor;
        String text = "RingBuffer wrote wrong color to idx=" + String(i);
        TEST_ASSERT_TRUE_MESSAGE(test == leds[i], text.c_str());
    }
}

void test_fill_rainbow() {
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds, 16, offset);
    CRGBArray<16> check;

    // rainbow pattern
    check.fill_rainbow(0,50);
    dut.fill_rainbow(0, 50);

    verifyCheckAgainstDut(check, dut);
}

void test_fill_gradient() {
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds, 16, offset);
    CRGBArray<16> check;

    //gradient tests
    CHSV c1 = CHSV(0, 255, 255);
    CHSV c2 = CHSV(32, 255, 255);
    CHSV c3 = CHSV(64, 255, 255);
    CHSV c4 = CHSV(96, 255, 255);

    // 2 color vesion
    check.fill_gradient(c1, c4);
    dut.fill_gradient(c1, c4);

    verifyCheckAgainstDut(check, dut);

    // 3 color vesion
    check.fill_gradient(c1, c2, c4);
    dut.fill_gradient(c1, c2, c4);
    
    verifyCheckAgainstDut(check, dut);

    // 4 color vesion
    check.fill_gradient(c1, c2, c3, c4);
    dut.fill_gradient(c1, c2, c3, c4);
    
    verifyCheckAgainstDut(check, dut);
}

void test_fill_gradient_RGB() {
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds, 16, offset);
    CRGBArray<16> check;

    //gradient tests
    CRGB c1 = CRGB(255,   0,   0);
    CRGB c2 = CRGB(0,   255,   0);
    CRGB c3 = CRGB(0,     0, 255);
    CRGB c4 = CRGB(255, 255, 255);
    
    // 2 color version
    check.fill_gradient_RGB(c1, c4);
    dut.fill_gradient_RGB(c1, c4);
    
    verifyCheckAgainstDut(check, dut);

    // 3 color version
    check.fill_gradient_RGB(c1, c3, c4);
    dut.fill_gradient_RGB(c1, c3, c4);
    
    verifyCheckAgainstDut(check, dut);

    // 4 color version
    check.fill_gradient_RGB(c1, c2, c3, c4);
    dut.fill_gradient_RGB(c1, c2, c3, c4);
    
    verifyCheckAgainstDut(check, dut);
}

void test_iterator_forward() {
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds, 16, offset);

    TEST_ASSERT_EQUAL_MESSAGE(16, dut.size(), "RingBuffer returned incorrect size");
    TEST_ASSERT_FALSE_MESSAGE(dut.reversed(), "RingBuffer is reversed");

    uint8_t test = offset;
    uint8_t touched = 0;
    for(auto it=dut.begin();it!=dut.end();it++, test++, touched++) {
        // access to it
        test = mod(test, dut.size());
        String text = "RingBuffer iterator returned wrong ref at idx=" + String(touched);
        TEST_ASSERT_EQUAL_MESSAGE(test * 10, it->r, text.c_str());
    }
    TEST_ASSERT_EQUAL_MESSAGE(dut.size(), touched, "Not all LEDS were visited by the iterator");
    
    uint8_t i = 0;
    for(auto &px : dut) {
        px.g = i++;
    }

    for(uint8_t i=0;i<dut.size();i++) {
        String text = "RingBuffer iterator failed the reange-based for loop at idx=" + String(i);
        TEST_ASSERT_EQUAL_MESSAGE(i, dut[i].g, text.c_str());
    }
}

void test_iterator_reverse() {
    constexpr int offset = 5;
    CPixelRingBuffer<CRGB> dut(leds, 15, 0, offset);

    TEST_ASSERT_EQUAL_MESSAGE(16, dut.size(), "RingBuffer returned incorrect size");
    TEST_ASSERT_TRUE_MESSAGE(dut.reversed(), "RingBuffer is not reversed");

    uint8_t i = 0;
    for(auto it=dut.begin();it!=dut.end();it++, i++) {
        // access to it
        uint8_t r = mod(dut.size() - 1 - (i + offset), dut.size()) * 10;
        String text = "RingBuffer iterator returned wrong ref at idx=" + String(i);
        TEST_ASSERT_EQUAL_MESSAGE(r, it->r, text.c_str());
    }
    
    i = 0;
    for(auto &px : dut) {
        px.g = i++;
    }

    for(uint8_t i=0;i<dut.size();i++) {
        String text = "RingBuffer iterator failed the reange-based for loop at idx=" + String(i);
        TEST_ASSERT_EQUAL_MESSAGE(i, dut[i].g, text.c_str());
    }

}

void setup()
{
    Serial.begin(115200);
    delay(2000); // service delay
    UNITY_BEGIN();

    RUN_TEST(test_basic_buffer);
    RUN_TEST(test_basic_reverse);
    RUN_TEST(test_basic_offset);
    RUN_TEST(test_complex_usage);

    RUN_TEST(test_fill_solid);
    RUN_TEST(test_fill_rainbow);
    RUN_TEST(test_fill_gradient);
    RUN_TEST(test_fill_gradient_RGB);

    RUN_TEST(test_iterator_forward);
    RUN_TEST(test_iterator_reverse);

    UNITY_END(); // stop unit testing
}

void loop()
{
}