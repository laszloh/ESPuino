#include <Arduino.h>
#include <unity.h>
#include <array>
#include "mock_fs.hpp"

#include "Playlist.hpp"

size_t allocCount = 0;
size_t deAllocCount = 0;
size_t reAllocCount = 0;

size_t heap;
size_t psram;

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

FolderPlaylistAlloc<UnitTestAllocator> *folderPlaylist;

void get_free_memory(void) {
    heap = ESP.getFreeHeap();
    psram = ESP.getFreePsram();
}

void test_free_memory(void) {
    size_t cHeap = ESP.getFreeHeap();
    size_t cPsram = ESP.getFreePsram();

    TEST_ASSERT_INT_WITHIN_MESSAGE(4, heap, cHeap, "Free heap after test (delta = 4 byte)");
    TEST_ASSERT_INT_WITHIN_MESSAGE(4, psram, cPsram, "Free psram after test (delta = 4 byte)");
}

// set stuff up here, this function is before a test function
void setUp(void) {
    allocCount = deAllocCount = reAllocCount = 0;
    get_free_memory();
}

void tearDown(void) {
    test_free_memory();
}

void setup_static(void) {
    folderPlaylist = new FolderPlaylistAlloc<UnitTestAllocator>();
}

void test_folder_alloc(void) {
    TEST_ASSERT_TRUE(folderPlaylist->reserve(10));

    folderPlaylist->clear();

    test_free_memory();

    // test memory actions
    TEST_ASSERT_EQUAL_MESSAGE(0, allocCount, "Calls to malloc");
    TEST_ASSERT_EQUAL_MESSAGE(1, deAllocCount, "Calls to free");
    TEST_ASSERT_EQUAL_MESSAGE(1, reAllocCount, "Calls to realloc");
}

void test_folder_content_absolute(void) {
    constexpr std::array<const char*, 6> contentAbsolute PROGMEM = {{
        "/sdcard/music/folderA/song1.mp3",
        "/sdcard/music/folderA/song2.mp3",
        "/sdcard/music/folderB/song3.mp3",
        "/sdcard/music/folderC/song4.mp3",
        "/sdcard/music/folderD/song5.mp3",
        "/sdcard/music/folderA/song6.mp3",
    }};

    folderPlaylist->clear();
    TEST_ASSERT_TRUE(folderPlaylist->reserve(contentAbsolute.size()));
    for(auto e : contentAbsolute) {
        TEST_ASSERT_TRUE(folderPlaylist->push_back(e));
    }
    TEST_ASSERT_EQUAL(contentAbsolute.size(), folderPlaylist->size());

    for(size_t i=0;i<contentAbsolute.size();i++){
       TEST_ASSERT_EQUAL_STRING(contentAbsolute[i], folderPlaylist->getAbsolutPath(i).c_str());
    }

    folderPlaylist->clear();

    // test memory actions
    TEST_ASSERT_EQUAL_MESSAGE(contentAbsolute.size(), allocCount, "Calls to malloc");
    TEST_ASSERT_EQUAL_MESSAGE(1 + contentAbsolute.size(), deAllocCount, "Calls to free");
    TEST_ASSERT_EQUAL_MESSAGE(1, reAllocCount, "Calls to realloc");
}

void test_folder_content_relative(void) {
    constexpr const char *basePath = "/sdcard/music/folderX";
    constexpr std::array<const char*, 4> contentRelative PROGMEM = {{
        "/sdcard/music/folderX/song1.mp3",
        "/sdcard/music/folderX/song2.mp3",
        "/sdcard/music/folderX/song3.mp3",
        "/sdcard/music/folderX/song4.mp3",
    }};

    folderPlaylist->clear(); // <-- nop operation
    TEST_ASSERT_TRUE(folderPlaylist->setBase(basePath));
    TEST_ASSERT_TRUE(folderPlaylist->reserve(contentRelative.size()));

    for(auto e : contentRelative) {
        TEST_ASSERT_TRUE(folderPlaylist->push_back(e));
    }
    TEST_ASSERT_EQUAL(contentRelative.size(), folderPlaylist->size());

    for(size_t i=0;i<contentRelative.size();i++){
       TEST_ASSERT_EQUAL_STRING(contentRelative[i], folderPlaylist->getAbsolutPath(i).c_str());
    }

    // this tests should fail
    constexpr const char *wrongBasePath PROGMEM = "/sdcard/music/folderZ/song1.mp3";
    constexpr const char *noMusicFile PROGMEM = "/sdcard/music/folderX/song4.doc";

    TEST_ASSERT_FALSE(folderPlaylist->push_back(wrongBasePath));
    TEST_ASSERT_FALSE(folderPlaylist->push_back(noMusicFile));

    // cleanup
    folderPlaylist->clear();

    // test memory actions
    TEST_ASSERT_EQUAL_MESSAGE(1 + contentRelative.size(), allocCount, "Calls to malloc");
    TEST_ASSERT_EQUAL_MESSAGE(contentRelative.size() + 1 + 1, deAllocCount, "Calls to free");
    TEST_ASSERT_EQUAL_MESSAGE(1, reAllocCount, "Calls to realloc");
}

void test_folder_content_automatic(void) {
    // this test will access a mock file system implementation
    Node musicFolder = {
        .fullPath = "/sdcard/music/folderE",
        .valid = true,
        .isDir = true,
        .size = 0,
        .content = {
                {
                    .fullPath = "/sdcard/music/folderE/song1.mp3",
                    .valid = true,
                    .isDir = false,
                    .size = 12345,
                    .content = std::list<Node>()
                },
                {
                    .fullPath = "/sdcard/music/folderE/song2.mp3",
                    .valid = true,
                    .isDir = false,
                    .size = 12345,
                    .content = std::list<Node>()
                },
                {
                    .fullPath = "/sdcard/music/folderE/song3.mp3",
                    .valid = true,
                    .isDir = false,
                    .size = 12345,
                    .content = std::list<Node>()
                },
                {
                    .fullPath = "/sdcard/music/folderE/song4.mp3",
                    .valid = true,
                    .isDir = false,
                    .size = 12345,
                    .content = std::list<Node>()
                },
                {
                    .fullPath = "/sdcard/music/folderE/A Folder",
                    .valid = true,
                    .isDir = true,
                    .size = 0,
                    .content = std::list<Node>()
                },
                {
                    .fullPath = "/sdcard/music/folderE/song5.mp3",
                    .valid = true,
                    .isDir = false,
                    .size = 12345,
                    .content = std::list<Node>()
                },
                {
                    .fullPath = "/sdcard/music/folderE/song6.mp3",
                    .valid = true,
                    .isDir = false,
                    .size = 12345,
                    .content = std::list<Node>()
                },
            }
    };
    constexpr size_t numFiles = 6;
    File root(musicFolder);

    folderPlaylist->createFromFolder(root);
    TEST_ASSERT_EQUAL_MESSAGE(numFiles, folderPlaylist->size(), "Number of elements in Playlist");

    size_t i=0;
    for(auto it=musicFolder.content.begin();it!=musicFolder.content.end();it++){
        if(!it->isDir) {
            TEST_ASSERT_EQUAL_STRING(it->fullPath.c_str(), folderPlaylist->getAbsolutPath(i).c_str());
            i++;
        }
    }

    // cleanup
    folderPlaylist->clear();

     // test memory actions
    TEST_ASSERT_EQUAL_MESSAGE(1 + numFiles, allocCount, "Calls to malloc");
    TEST_ASSERT_EQUAL_MESSAGE(8, deAllocCount, "Calls to free");
    TEST_ASSERT_EQUAL_MESSAGE(2, reAllocCount, "Calls to realloc");
}

void setup()
{
    Serial.begin(115200);
    delay(2000); // service delay
    UNITY_BEGIN();

    setup_static();

    RUN_TEST(test_folder_alloc);
    RUN_TEST(test_folder_content_absolute);
    RUN_TEST(test_folder_content_relative);
    RUN_TEST(test_folder_content_automatic);


    UNITY_END(); // stop unit testing
}

void loop()
{
}
