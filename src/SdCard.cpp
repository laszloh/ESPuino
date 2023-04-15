#include <Arduino.h>
#include "settings.h"

#include "SdCard.h"

#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"
#include "playlists/FolderPlaylist.hpp"
#include "playlists/WebstreamPlaylist.hpp"

#include <esp_random.h>
#include <esp_vfs_fat.h>

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

// Takes a directory as input and returns a random subdirectory from it
const String SdCard_pickRandomSubdirectory(const char *_directory) {
	constexpr bool fileNameSupport = (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 8));
	const uint32_t listStartTimestamp = millis();

	// Look if file/folder requested really exists. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory) {
		// does not exists
		Log_Println(dirOrFileDoesNotExist, LOGLEVEL_ERROR);
		return String {};
	}
	Log_Printf(LOGLEVEL_NOTICE, tryToPickRandomDir, _directory);

	size_t dirCount = 0;
	while (true) {
		bool isDir;
		if constexpr (fileNameSupport) {
			const String path = directory.getNextFileName(&isDir);
			if (path.isEmpty()) {
				break;
			}
		} else {
			File fileItem = directory.openNextFile();
			if (!fileItem) {
				break;
			}
			isDir = fileItem.isDirectory();
		}
		if (isDir) {
			dirCount++;
		}
	}
	if (!dirCount) {
		// no paths in folder
		return String {};
	}

	const uint32_t randomNumber = esp_random() % dirCount;
	String path;
	for (size_t i = 0; i < randomNumber;) {
		bool isDir;
		if constexpr (fileNameSupport) {
			path = directory.getNextFileName(&isDir);
		} else {
			File fileItem = directory.openNextFile();
			if (!fileItem) {
				path = "";
			} else {
				path = getPath(fileItem);
				isDir = fileItem.isDirectory();
			}
		}
		if (path.isEmpty()) {
			// we reached the end before finding the correct dir!
			return String {};
		}
		if (isDir) {
			i++;
		}
	}
	Log_Printf(LOGLEVEL_NOTICE, pickedRandomDir, path.c_str());
	Log_Printf(LOGLEVEL_DEBUG, "pick random directory from SD-card finished: %lu ms", (millis() - listStartTimestamp));
	return path;
}

static std::unique_ptr<Playlist> SdCard_ParseM3UPlaylist(File f, bool forceExtended = false) {
	const String line = f.readStringUntil('\n');
	bool extended = line.startsWith("#EXTM3U") || forceExtended;
	auto playlist = std::make_unique<FolderPlaylist>();

	if (extended) {
		// extended m3u file format
		// ignore all lines starting with '#'

		while (f.available()) {
			String line = f.readStringUntil('\n');
			if (!line.startsWith("#")) {
				// this something we have to save
				line.trim();
				if (!playlist->push_back(line)) {
					return nullptr;
				}
			}
		}
		// resize memory to fit our count
		playlist->compress();
		return playlist;
	}

	// normal m3u is just a bunch of filenames, 1 / line
	f.seek(0);
	while (f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if (!playlist->push_back(line)) {
			return nullptr;
		}
	}
	// resize memory to fit our count
	playlist->compress();
	return playlist;
}

/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
std::unique_ptr<Playlist> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode, const uint8_t _maxRecursionDepth, bool _recursionMode) {
	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Println(dirOrFileDoesNotExist, LOGLEVEL_ERROR);
		return nullptr;
	}

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U) {
		if (fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size()) {
			// create a m3u playlist and parse the file
			return SdCard_ParseM3UPlaylist(fileOrDirectory);
		}
		// if we reach here, we failed
		return nullptr;
	}

	// File-mode
	if (!fileOrDirectory.isDirectory()) {
		const char *path = getPath(fileOrDirectory);
		if (Playlist::fileValid(path)) {
			return std::make_unique<WebstreamPlaylist>(path);
		}
	}

	// Folder mode
	auto playlist = std::make_unique<FolderPlaylist>();
	playlist->createFromFolder(fileOrDirectory);
	if (!playlist->isValid()) {
		// something went wrong
		Log_Println(unableToAllocateMemForLinearPlaylist, LOGLEVEL_ERROR);
		return nullptr;
	}

	// we are finished
	Log_Printf(LOGLEVEL_NOTICE, numberOfValidFiles, playlist->size());
	return playlist;
}

// Extracts basepath out of a given filepath
std::string_view SdCard_Basepath(const char *filepath) {
	if (!filepath) {
		return std::string_view();
	}
	std::string_view str(filepath);
	auto pos = str.find_last_of('/');
	if (pos == std::string::npos) {
		return std::string_view();
	}
	return str.substr(0, pos + 1);
}

// Used for recursive playmodes. Allows to jump forwards and backwards between folders using
// CMD_PREVFOLDER (backwards) and CMD_NEXTFOLDER (forwards) to previous / next folder in playlist.
// Returns -1 if no prev or next folder was found or no playlist is available
// Returns >=0 if folderjump is possible. Number represents the index of the current playlist's track to jump to.
int16_t SdCard_findNextOrPrevDirectoryTrack(const Playlist &_playlist, size_t currentTrackIndexInPlaylist, SearchDirection direction) {
	// Look if index requested is out of bounds
	if (currentTrackIndexInPlaylist >= _playlist.size()) {
		return -1;
	}

	const auto currentFile = _playlist.getAbsolutePath(currentTrackIndexInPlaylist);
	std::string_view basepathOfCurrentTrack = SdCard_Basepath(currentFile.c_str()); // Get basepath of current track

	// Look forwards
	if (direction == SearchDirection::Forward) {
		for (uint16_t i = currentTrackIndexInPlaylist + 1; i < _playlist.size(); i++) {
			const auto nextFile = _playlist.getAbsolutePath(i);
			std::string_view basepathOfTrackToLookUp = SdCard_Basepath(nextFile.c_str());
			if (basepathOfTrackToLookUp != basepathOfCurrentTrack) {
				Log_Printf(LOGLEVEL_DEBUG, jumpForwardsToFolder, basepathOfTrackToLookUp.data(), "\n");
				return i; // Return first track after basepath change
			}
		}

		// Look backwards
	} else if (direction == SearchDirection::Backward) {
		//  Go back as long as we don't hit 0
		if (!currentTrackIndexInPlaylist) {
			return currentTrackIndexInPlaylist;
		}

		if (currentTrackIndexInPlaylist != 1) {
			for (uint16_t i = currentTrackIndexInPlaylist - 1; i > 0; i--) {
				const auto prevFile = _playlist.getAbsolutePath(i);
				std::string_view basepathOfTrackToLookUp = SdCard_Basepath(prevFile.c_str());
				if (basepathOfTrackToLookUp != basepathOfCurrentTrack) {
					for (uint16_t j = i - 1; j > 0; j--) {
						const auto prevInnerFile = _playlist.getAbsolutePath(j);
						std::string_view basepathOfTrackToLookUpInner = SdCard_Basepath(prevInnerFile.c_str());
						if (basepathOfTrackToLookUpInner != basepathOfTrackToLookUp) { // ...but keep on looking for the 2nd change...
							Log_Printf(LOGLEVEL_DEBUG, jumpBackwardsToFolder, basepathOfTrackToLookUpInner.data(), "\n");
							return j + 1; // ...just to add +1 to get the previous element before the 2nd change
						}
					}
					Log_Printf(LOGLEVEL_DEBUG, jumpForwardsToFolder, basepathOfTrackToLookUp.data(), "\n");
					return i; // Return first track after basepath change
				}
			}
		} else {
			return 0;
		}
	}

	// If no jump possible, return -1
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
