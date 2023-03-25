#include <Arduino.h>
#include <unity.h>
#include <array>
#include <string>
#include "../mock_fs.hpp"

#include "playlists/CacheFilePlaylist.hpp"

size_t allocCount = 0;
size_t deAllocCount = 0;
size_t reAllocCount = 0;

size_t heap;
size_t psram;

// our mock file system
mockfs::Node musicFolder = {
    .fullPath = "/sdcard/music/folderE",
    .isDir = true,
    .content = std::vector<uint8_t>(),
    .files = {
        {
            .fullPath = "/sdcard/music/folderE/song1.mp3",
            .isDir = false,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
        {
            .fullPath = "/sdcard/music/folderE/song2.mp3",
            .isDir = false,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
        {
            .fullPath = "/sdcard/music/folderE/song3.mp3",
            .isDir = false,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
        {
            .fullPath = "/sdcard/music/folderE/song4.mp3",
            .isDir = false,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
        {
            .fullPath = "/sdcard/music/folderE/A Folder",
            .isDir = true,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
        {
            .fullPath = "/sdcard/music/folderE/song5.mp3",
            .isDir = false,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
        {
            .fullPath = "/sdcard/music/folderE/song6.mp3",
            .isDir = false,
            .content = std::vector<uint8_t>(),
            .files = std::vector<mockfs::Node>()
        },
    }
};

// mock allocator
struct UnitTestAllocator {
  void* allocate(size_t size) {
    void *ret;
	if(psramInit()) {
		ret = ps_malloc(size);
	} else {
    	ret = malloc(size);
	}
    if(ret) {
        allocCount++;
    }
    return ret;
  }

  void deallocate(void* ptr) {
    free(ptr);
    deAllocCount ++;
  }

  void* reallocate(void* ptr, size_t new_size) {
    void *ret;
	if(psramInit()) {
		ret = ps_realloc(ptr, new_size);
	} else {
    	ret = realloc(ptr, new_size);
	}
    if(ret)
        reAllocCount++;
    return ret;
  }
};

CacheFilePlaylistAlloc<UnitTestAllocator> *cachePlaylist;

#define get_free_memory() do {      \
    heap = ESP.getFreeHeap();       \
    psram = ESP.getFreePsram();     \
}while(0);                          \

#define test_free_memory() do {         \
    size_t cHeap = ESP.getFreeHeap();   \
    size_t cPsram = ESP.getFreePsram(); \
    TEST_ASSERT_INT_WITHIN_MESSAGE(4, heap, cHeap, "Free heap after test (delta = 4 byte)");        \
    TEST_ASSERT_INT_WITHIN_MESSAGE(4, psram, cPsram, "Free psram after test (delta = 4 byte)");     \
}while(0);

void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		Serial.printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			Serial.printf(" ");
			if ((i+1) % 16 == 0) {
				Serial.printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					Serial.printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					Serial.printf("   ");
				}
				Serial.printf("|  %s \n", ascii);
			}
		}
	}
}

// set stuff up here, this function is before a test function
void setUp(void) {
    allocCount = deAllocCount = reAllocCount = 0;
    // get_free_memory();   // we have a memory leak of ~204 bytes somewhere in the code
}

void tearDown(void) {
    // test_free_memory();
}

void setup_static(void) {
    cachePlaylist = new CacheFilePlaylistAlloc<UnitTestAllocator>();
}

void test_cache_read_write_absolute(void) {
    constexpr std::array<const char*, 6> contentAbsolute PROGMEM = {{
        "/sdcard/music/folderA/song1.mp3",
        "/sdcard/music/folderA/song2.mp3",
        "/sdcard/music/folderB/song3.mp3",
        "/sdcard/music/folderC/song4.mp3",
        "/sdcard/music/folderD/song5.mp3",
        "/sdcard/music/folderA/song6.mp3",
    }};

    mockfs::Node cacheNode = mockfs::Node::empty("/sdcard/music/test.csv");
    File cacheFile = mockfs::MockFileImp::open(&cacheNode, false);

    cachePlaylist->clear();

    TEST_ASSERT_TRUE(cachePlaylist->reserve(contentAbsolute.size()));
    for(auto e : contentAbsolute) {
        TEST_ASSERT_TRUE(cachePlaylist->push_back(e));
    }
    TEST_ASSERT_EQUAL(contentAbsolute.size(), cachePlaylist->size());

    // push the data into the cache file
    bool serialize = cachePlaylist->serialize(cacheFile);
    TEST_ASSERT_TRUE_MESSAGE(serialize, "Serialize failed to write file");

    // dump data
    log_n("Wrote: %d, dump:", cacheFile.size());
    DumpHex(cacheNode.content.data(), cacheNode.content.size());

    // destory cachefile
    cachePlaylist->clear();

    // read back the data
    bool deserialize = cachePlaylist->deserialize(cacheFile);
    TEST_ASSERT_TRUE_MESSAGE(deserialize, "Deserialize failed to read file");

    for(size_t i=0;i<cachePlaylist->size();i++) {
        TEST_ASSERT_EQUAL_STRING(contentAbsolute[i], cachePlaylist->getAbsolutePath(i).c_str());
    }

    // destory cachefile
    cachePlaylist->clear();
    cacheFile.close();
}

void test_cache_read_write_relative(void) {
    constexpr const char *basePath = "/sdcard/music/folderX";
    constexpr std::array<const char*, 4> contentRelative PROGMEM = {{
        "/sdcard/music/folderX/song1.mp3",
        "/sdcard/music/folderX/song2.mp3",
        "/sdcard/music/folderX/song3.mp3",
        "/sdcard/music/folderX/song4.mp3",
    }};

    mockfs::Node cacheNode = mockfs::Node::empty("/sdcard/music/test.csv");
    File cacheFile = mockfs::MockFileImp::open(&cacheNode, false);

    cachePlaylist->clear(); // <-- nop operation
    TEST_ASSERT_TRUE(cachePlaylist->setBase(basePath));
    TEST_ASSERT_TRUE(cachePlaylist->reserve(contentRelative.size()));

    for(auto e : contentRelative) {
        TEST_ASSERT_TRUE(cachePlaylist->push_back(e));
    }
    TEST_ASSERT_EQUAL(contentRelative.size(), cachePlaylist->size());

    for(size_t i=0;i<contentRelative.size();i++){
       TEST_ASSERT_EQUAL_STRING(contentRelative[i], cachePlaylist->getAbsolutePath(i).c_str());
    }

    // push the data into the cache file
    bool serialize = cachePlaylist->serialize(cacheFile);
    TEST_ASSERT_TRUE_MESSAGE(serialize, "Serialize failed to write file");

    // dump data
    log_n("Wrote: %d, dump:", cacheFile.size());
    DumpHex(cacheNode.content.data(), cacheNode.content.size());

    // destory cachefile
    cachePlaylist->clear();

    // read back the data
    bool deserialize = cachePlaylist->deserialize(cacheFile);
    TEST_ASSERT_TRUE_MESSAGE(deserialize, "Deserialize failed to read file");

    for(size_t i=0;i<cachePlaylist->size();i++) {
        TEST_ASSERT_EQUAL_STRING(contentRelative[i], cachePlaylist->getAbsolutePath(i).c_str());
    }

    // destory cachefile
    cachePlaylist->clear();
    cacheFile.close();

    // this tests should fail
    constexpr const char *wrongBasePath PROGMEM = "/sdcard/music/folderZ/song1.mp3";
    constexpr const char *noMusicFile PROGMEM = "/sdcard/music/folderX/song4.doc";

    TEST_ASSERT_FALSE(cachePlaylist->push_back(wrongBasePath));
    TEST_ASSERT_FALSE(cachePlaylist->push_back(noMusicFile));

    // cleanup
    cachePlaylist->clear();
}

void test_cache_read_tests(void) {
    constexpr std::array<const char*, 4> contentRelative PROGMEM = {{
        "/sdcard/music/folderX/song1.mp3",
        "/sdcard/music/folderX/song2.mp3",
        "/sdcard/music/folderX/song3.mp3",
        "/sdcard/music/folderX/song4.mp3",
    }};

    mockfs::Node cacheNode = mockfs::Node::fromBuffer("/sdcard/music/test.csv", reinterpret_cast<const uint8_t*>(
        "CF\x00\x01\x00\x01\x00\x00\x00\x04\xE1\x99\x58\x79#"
        "/sdcard/music/folderX#"
        "/sdcard/music/folderX/song1.mp3#"
        "/sdcard/music/folderX/song2.mp3#"
        "/sdcard/music/folderX/song3.mp3#"
        "/sdcard/music/folderX/song4.mp3#"),
        165
    );
    File cacheFile = mockfs::MockFileImp::open(&cacheNode, false);

    log_n("Wrote: %d, dump:", cacheFile.size());
    DumpHex(cacheNode.content.data(), cacheNode.content.size());

    // read back the data
    TEST_ASSERT_TRUE_MESSAGE(cachePlaylist->deserialize(cacheFile), "Deserialize failed to read file");

    // check against test 
    for(size_t i=0;i<cachePlaylist->size();i++) {
        TEST_ASSERT_EQUAL_STRING(contentRelative[i], cachePlaylist->getAbsolutePath(i).c_str());
    }

    // destroy file header
    cacheNode.content[5] = 0xFF;

    // read back the data
    TEST_ASSERT_FALSE_MESSAGE(cachePlaylist->deserialize(cacheFile), "Deserialize accepted corrupted header");

    // cleanup
    cachePlaylist->clear();
}

void test_cache_read_old_playlist() {
    constexpr std::array<const char*, 5> testData PROGMEM = {{
        "/sdcard/music/folderX/song1.mp3",
        "/sdcard/music/folderX/song2.mp3",
        "/sdcard/music/folderX/song3.mp3",
        "/sdcard/music/folderX/song4.mp3",
        "/sdcard/music/folderX/song5.mp3",
    }};

    mockfs::Node cacheNode = mockfs::Node::fromBuffer("/sdcard/music/test.csv", reinterpret_cast<const uint8_t*>(
        "/sdcard/music/folderX/song1.mp3#"
        "/sdcard/music/folderX/song2.mp3#"
        "/sdcard/music/folderX/song3.mp3#"
        "/sdcard/music/folderX/song4.mp3#"
        "/sdcard/music/folderX/song5.mp3#"),
        160
    );
    File cacheFile = mockfs::MockFileImp::open(&cacheNode, false);

    TEST_ASSERT_TRUE_MESSAGE(cachePlaylist->isOldPlaylist(cacheFile), "Cache playlist did not ID the file as an old playlist");
    TEST_ASSERT_TRUE_MESSAGE(cachePlaylist->deserializeOldPlaylist(cacheFile), "Could not parse old cache format");

    // check against test 
    for(size_t i=0;i<cachePlaylist->size();i++) {
        TEST_ASSERT_EQUAL_STRING(testData[i], cachePlaylist->getAbsolutePath(i).c_str());
    }

    // write new cache format
    cacheNode.content.clear();

    TEST_ASSERT_TRUE_MESSAGE(cachePlaylist->serialize(cacheFile), "Filed to write new format");

    log_n("Wrote: %d, dump:", cacheFile.size());
    DumpHex(cacheNode.content.data(), cacheNode.content.size());

    // read back the data
    TEST_ASSERT_FALSE_MESSAGE(cachePlaylist->deserialize(cacheFile), "Deserialize failed to read file");
}

void setup()
{
    Serial.begin(115200);
    delay(2000); // service delay
    UNITY_BEGIN();

    setup_static();

    RUN_TEST(test_cache_read_write_absolute);
    RUN_TEST(test_cache_read_write_relative);
    RUN_TEST(test_cache_read_tests);
    RUN_TEST(test_cache_read_old_playlist);


    UNITY_END(); // stop unit testing
}

void loop()
{
}
