#include <Arduino.h>
#include "settings.h"

#include "SdCard.h"

#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#include <algorithm>
#include <esp_random.h>
#include <esp_vfs_fat.h>
#include <string_view>

#ifdef SD_MMC_1BIT_MODE
fs::FS gFSystem = (fs::FS) SD_MMC;
#else
SPIClass spiSD(HSPI);
fs::FS gFSystem = (fs::FS) SD;
#endif

uint8_t maxRecursionDepth;

void SdCard_Init(void) {
#ifdef NO_SDCARD
	// Initialize without any SD card, e.g. for webplayer only
	Log_Println("Init without SD card ", LOGLEVEL_NOTICE);
	return
#endif

#ifndef SINGLE_SPI_ENABLE
	#ifdef SD_MMC_1BIT_MODE
		pinMode(2, INPUT_PULLUP);
	while (!SD_MMC.begin("/sdcard", true)) {
	#else
		pinMode(SPISD_CS, OUTPUT);
	digitalWrite(SPISD_CS, HIGH);
	spiSD.begin(SPISD_SCK, SPISD_MISO, SPISD_MOSI, SPISD_CS);
	spiSD.setFrequency(1000000);
	while (!SD.begin(SPISD_CS, spiSD)) {
	#endif
#else
	#ifdef SD_MMC_1BIT_MODE
	pinMode(2, INPUT_PULLUP);
	while (!SD_MMC.begin("/sdcard", true)) {
	#else
	while (!SD.begin(SPISD_CS)) {
	#endif
#endif
		Log_Println(unableToMountSd, LOGLEVEL_ERROR);
		delay(500);
#ifdef SHUTDOWN_IF_SD_BOOT_FAILS
		if (millis() >= deepsleepTimeAfterBootFails * 1000) {
			Log_Println(sdBootFailedDeepsleep, LOGLEVEL_ERROR);
			esp_deep_sleep_start();
		}
#endif
	}

	// Used when building recursive playlists
	maxRecursionDepth = gPrefsSettings.getUInt("nvsRecDepth", 255);
	if (maxRecursionDepth == 255) {
		gPrefsSettings.putUInt("nvsRecDepth", 2);
		maxRecursionDepth = 2;
	}
}

void SdCard_Exit(void) {
// SD card goto idle mode
#ifdef SINGLE_SPI_ENABLE
	Log_Println("shutdown SD card (SPI)..", LOGLEVEL_NOTICE);
	SD.end();
#endif
#ifdef SD_MMC_1BIT_MODE
	Log_Println("shutdown SD card (SD_MMC)..", LOGLEVEL_NOTICE);
	SD_MMC.end();
#endif
}

sdcard_type_t SdCard_GetType(void) {
	sdcard_type_t cardType;
#ifdef SD_MMC_1BIT_MODE
	Log_Println(sdMountedMmc1BitMode, LOGLEVEL_NOTICE);
	cardType = SD_MMC.cardType();
#else
	Log_Println(sdMountedSpiMode, LOGLEVEL_NOTICE);
	cardType = SD.cardType();
#endif
	return cardType;
}

uint64_t SdCard_GetSize() {
#ifdef SD_MMC_1BIT_MODE
	return SD_MMC.cardSize();
#else
	return SD.cardSize();
#endif
}

uint64_t SdCard_GetFreeSize() {
#ifdef SD_MMC_1BIT_MODE
	return SD_MMC.cardSize() - SD_MMC.usedBytes();
#else
	return SD.cardSize() - SD.usedBytes();
#endif
}

uint8_t SdCard_GetMaxRecursionDepth(void) {
	return maxRecursionDepth;
}

// Returns recursion depth that's used then playlists are generated for recursive playmodes
size_t SdCard_SetMaxRecursionDepth(uint8_t _maxRecursionDepth) {
	maxRecursionDepth = _maxRecursionDepth;
	return gPrefsSettings.putUInt("nvsRecDepth", SdCard_GetMaxRecursionDepth());
}

void SdCard_PrintInfo() {
	// show SD card type
	sdcard_type_t cardType = SdCard_GetType();
	const char *type = "UNKNOWN";
	switch (cardType) {
		case CARD_MMC:
			type = "MMC";
			break;

		case CARD_SD:
			type = "SDSC";
			break;

		case CARD_SDHC:
			type = "SDHC";
			break;

		default:
			break;
	}
	Log_Printf(LOGLEVEL_DEBUG, "SD card type: %s", type);
	// show SD card size / free space
	uint64_t cardSize = SdCard_GetSize() / (1024 * 1024);
	uint64_t freeSize = SdCard_GetFreeSize() / (1024 * 1024);
	;
	Log_Printf(LOGLEVEL_NOTICE, sdInfo, cardSize, freeSize);
}

// Check if file-type is correct
bool fileValid(std::string_view _fileItem) {
	// clang-format off
	// all supported extension
	constexpr std::string_view audioFileSufix[] = {
		".mp3",
		".aac",
		".m4a",
		".wav",
		".flac",
		".ogg",
		".oga",
		".opus",
		// playlists
		".m3u",
		".m3u8",
		".pls",
		".asx"
	};
	// clang-format on
	constexpr size_t maxExtLen = std::max_element(std::begin(audioFileSufix), std::end(audioFileSufix), [](std::string_view a, std::string_view b) {
		return a.size() < b.size();
	})->size();

	if (_fileItem.empty()) {
		// invalid entry
		return false;
	}

	// check for streams
	if (_fileItem.starts_with("http://") || _fileItem.starts_with("https://")) {
		// this is a stream
		return true;
	}

	// extract filename from path
	size_t lastSlashPos = _fileItem.find_last_of('/');
	std::string_view filename = (lastSlashPos == std::string_view::npos) ? _fileItem : _fileItem.substr(lastSlashPos + 1);
	if (filename.empty()) {
		// invalid entry
		return false;
	}

	// check for a hidden file (filename starts with a dot)
	if (filename[0] == '.') {
		return false;
	}

	// extract the file extension
	size_t dotPos = filename.find_last_of('.');
	if (dotPos == std::string_view::npos) {
		// no extension found
		return false;
	}
	std::string extension {filename.substr(dotPos)};

	// check extension length, if it's too long, it's definitly not supported
	if (extension.size() > maxExtLen) {
		return false;
	}

	// check extension against all supported values
	std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
		return std::tolower(c);
	});

	for (const auto &e : audioFileSufix) {
		if (extension == e) {
			// hit we found the extension
			return true;
		}
	}

	// Log_Printf(LOGLEVEL_DEBUG, "File not supported: %s", _fileItem);
	return false;
}

// Takes a directory as input and returns a random subdirectory from it
const String SdCard_pickRandomSubdirectory(const char *_directory) {
	// Look if folder requested really exists and is a folder. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory || !directory.isDirectory()) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, _directory);
		return String();
	}
	Log_Printf(LOGLEVEL_NOTICE, tryToPickRandomDir, _directory);

	// iterate through and count all dirs
	size_t dirCount = 0;
	while (1) {
		bool isDir;
		const String name = directory.getNextFileName(&isDir);
		if (name.isEmpty()) {
			break;
		}
		if (isDir) {
			dirCount++;
		}
	}
	if (!dirCount) {
		// no paths in folder
		return String();
	}

	const uint32_t randomNumber = esp_random() % dirCount;
	directory.rewindDirectory();
	dirCount = 0;
	while (1) {
		bool isDir;
		const String name = directory.getNextFileName(&isDir);
		if (name.isEmpty()) {
			break;
		}
		if (isDir) {
			if (dirCount == randomNumber) {
				return name;
			}
			dirCount++;
		}
	}

	// if we reached here, something went wrong
	return String();
}

static std::optional<std::unique_ptr<Playlist>> SdCard_ParseM3UPlaylist(File file) {
	std::unique_ptr<Playlist> playlist = std::make_unique<Playlist>();

	// reserve a sane amount of memory to reduce heap fragmentation
	playlist->reserve(64);
	// normal m3u is just a bunch of filenames, 1 / line
	// extended m3u file format can also include comments or special directives, prefaced by the "#" character
	// -> ignore all lines starting with '#'

	while (file.available()) {
		String line = file.readStringUntil('\n');
		if (!line.startsWith("#")) {
			// this something we have to save
			line.trim();
			// save string
			playlist->emplace_back(line.c_str());
		}
	}

	// resize std::vector memory to fit our count
	playlist->shrink_to_fit();
	return playlist;
}

static void SdCard_PopulatePlaylistWithDir(Playlist &list, const char *path, bool _recursionMode, const uint8_t _maxRecursionDepth, uint8_t &currentRecDepth, size_t &hiddenFiles) {
	File dir = gFSystem.open(path);
	if (!dir || !dir.isDirectory()) {
		Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, path);
		return;
	}

	do {
		bool isDir;
		const String name = dir.getNextFileName(&isDir);
		if (name.isEmpty()) {
			break;
		}
		if (isDir) {
			//  Jump into directory if recursion is allowed
			if (_recursionMode && currentRecDepth < _maxRecursionDepth) {
				currentRecDepth++;
				Log_Printf(LOGLEVEL_DEBUG, "Added folder: %s, depth of recursion: %d\n", name.c_str(), currentRecDepth);
				SdCard_PopulatePlaylistWithDir(list, name.c_str(), _recursionMode, _maxRecursionDepth, currentRecDepth, hiddenFiles);
				currentRecDepth--;
			}
			continue;
		}
		// Don't support filenames that start with "." and only allow .mp3 and other supported audio file formats
		if (fileValid(name.c_str())) {
			// save it to the vector
			list.emplace_back(name.c_str());
		} else {
			hiddenFiles++;
			Log_Printf(LOGLEVEL_DEBUG, "File is hidden or not supported: %s", name.c_str());
		}
	} while (true);
}

/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
std::optional<std::unique_ptr<Playlist>> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode, const uint8_t _maxRecursionDepth, bool _recursionMode) {
	try {
		// Look if file/folder requested really exists. If not => break.
		File fileOrDirectory = gFSystem.open(fileName);
		if (!fileOrDirectory) {
			Log_Printf(LOGLEVEL_ERROR, dirOrFileDoesNotExist, fileName);
			return std::nullopt;
		}

		// Parse m3u-playlist and create linear-playlist out of it
		if (_playMode == LOCAL_M3U) {
			if (!fileOrDirectory.isDirectory() && fileOrDirectory.size() > 0) {
				// function takes care of everything
				return SdCard_ParseM3UPlaylist(fileOrDirectory);
			}
			return std::nullopt;
		}

		// if we reach this code, it was not a m3u

		std::unique_ptr<Playlist> playlist = std::make_unique<Playlist>();

		// File-mode
		if (!fileOrDirectory.isDirectory()) {
			playlist->emplace_back(fileOrDirectory.path());
			return playlist;
		}

		// Directory-mode (linear-playlist)
		playlist->reserve(64); // reserve a sane amount of memory to reduce the number of reallocs
		size_t hiddenFiles = 0;
		uint8_t currentRecDepth = 0;

		Log_Printf(LOGLEVEL_DEBUG, freeMemory, ESP.getFreeHeap());
		Log_Printf(LOGLEVEL_NOTICE, playlistRecDepth, _maxRecursionDepth);
		SdCard_PopulatePlaylistWithDir(*playlist, fileOrDirectory.path(), _recursionMode, _maxRecursionDepth, currentRecDepth, hiddenFiles);

		playlist->shrink_to_fit();

		Log_Printf(LOGLEVEL_NOTICE, numberOfValidFiles, playlist->size());
		Log_Printf(LOGLEVEL_DEBUG, "Hidden files: %u", hiddenFiles);

		return playlist;
	} catch (const std::bad_alloc &e) {
		Log_Printf(LOGLEVEL_ERROR, "Out of memory building playlist for %s: %s", fileName, e.what());
		return std::nullopt;
	}
}

// Extracts basepath out of a given filepath
std::string_view SdCard_Basepath(std::string_view filepath) {
	auto pos = filepath.find_last_of('/');
	if (pos == std::string_view::npos) {
		return std::string_view();
	}
	return filepath.substr(0, pos + 1);
}

// Used for recursive playmodes. Allows to jump forwards and backwards between folders using
// CMD_PREVFOLDER (backwards) and CMD_NEXTFOLDER (forwards) to previous / next folder in playlist.
// Returns -1 if no prev or next folder was found or no playlist is available
// Returns >=0 if folderjump is possible. Number represents the index of the current playlist's track to jump to.
int16_t SdCard_findNextOrPrevDirectoryTrack(const Playlist &playlist, size_t currentIdx, SearchDirection direction) {
	if (currentIdx >= playlist.size() || playlist[currentIdx].empty()) {
		return -1;
	}
	const std::string_view currentBase = SdCard_Basepath(playlist[currentIdx]);

	switch (direction) {
		case SearchDirection::Forward: {
			for (size_t i = currentIdx + 1; i < playlist.size(); ++i) {
				const std::string_view base = SdCard_Basepath(playlist[i]);
				if (base != currentBase) {
					Log_Printf(LOGLEVEL_DEBUG, jumpForwardsToFolder, static_cast<int>(base.size()), base.data());
					return static_cast<int16_t>(i);
				}
			}
			return -1;
		}

		case SearchDirection::Backward: {
			// Single backward pass: find the first basepath change (= start of previous folder),
			// then keep walking until the next change (= folder before that). prevStart tracks the
			// first track of the previous folder so we can return it even when the playlist begins
			// with that folder (i.e. no second change exists).
			std::string_view prevBase;
			size_t prevStart = 0;
			bool inPrev = false;
			for (size_t i = currentIdx; i > 0; --i) {
				const size_t idx = i - 1;
				const std::string_view base = SdCard_Basepath(playlist[idx]);
				if (!inPrev) {
					if (base != currentBase) {
						prevBase = base;
						prevStart = idx;
						inPrev = true;
					}
				} else if (base != prevBase) {
					Log_Printf(LOGLEVEL_DEBUG, jumpBackwardsToFolder, static_cast<int>(prevBase.size()), prevBase.data());
					return static_cast<int16_t>(i);
				} else {
					prevStart = idx;
				}
			}
			if (inPrev) {
				Log_Printf(LOGLEVEL_DEBUG, jumpBackwardsToFolder, static_cast<int>(prevBase.size()), prevBase.data());
				return static_cast<int16_t>(prevStart);
			}
			return -1;
		}
	}

	return -1;
}

const String SdCard_GetVolumeLabel() {
#if FF_USE_LABEL
	char label[24];
	memset(label, 0, sizeof(label));

	DWORD vsn = 0;
	FRESULT res = f_getlabel("", label, &vsn);

	if (res == FR_OK && strlen(label) > 0) {
		return String(label);
	}
#endif
	return String("/");
}
