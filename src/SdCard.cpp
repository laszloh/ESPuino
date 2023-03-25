#include <Arduino.h>
#include "settings.h"
#include "SdCard.h"
#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

#include "playlists/FolderPlaylist.hpp"
#include "playlists/CacheFilePlaylist.hpp"
#include "playlists/M3UPlaylist.hpp"
#include "playlists/WebstreamPlaylist.hpp"

#ifdef SD_MMC_1BIT_MODE
	fs::FS gFSystem = (fs::FS)SD_MMC;
#else
	SPIClass spiSD(HSPI);
	fs::FS gFSystem = (fs::FS)SD;
#endif

void SdCard_Init(void) {
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
				Log_Println((char *) FPSTR(unableToMountSd), LOGLEVEL_ERROR);
				delay(500);
	#ifdef SHUTDOWN_IF_SD_BOOT_FAILS
				if (millis() >= deepsleepTimeAfterBootFails * 1000) {
					Log_Println((char *) FPSTR(sdBootFailedDeepsleep), LOGLEVEL_ERROR);
					esp_deep_sleep_start();
				}
	#endif
			}
}

void SdCard_Exit(void) {
	// SD card goto idle mode
	#ifdef SD_MMC_1BIT_MODE
		SD_MMC.end();
	#endif
}

sdcard_type_t SdCard_GetType(void) {
	sdcard_type_t cardType;
	#ifdef SD_MMC_1BIT_MODE
		Log_Println((char *) FPSTR(sdMountedMmc1BitMode), LOGLEVEL_NOTICE);
		cardType = SD_MMC.cardType();
	#else
		Log_Println((char *) FPSTR(sdMountedSpiMode), LOGLEVEL_NOTICE);
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

void SdCard_PrintInfo() {
	// show SD card type
	sdcard_type_t cardType = SdCard_GetType();
	Log_Print((char *) F("SD card type: "), LOGLEVEL_DEBUG, true );
	if (cardType == CARD_MMC) {
		Log_Println((char *) F("MMC"), LOGLEVEL_DEBUG);
	} else if (cardType == CARD_SD) {
		Log_Println((char *) F("SDSC"), LOGLEVEL_DEBUG);
	} else if (cardType == CARD_SDHC) {
		Log_Println((char *) F("SDHC"), LOGLEVEL_DEBUG);
	} else {
		Log_Println((char *) F("UNKNOWN"), LOGLEVEL_DEBUG);
	}
	// show SD card size / free space
	uint64_t cardSize = SdCard_GetSize() / (1024 * 1024);
	uint64_t freeSize = SdCard_GetFreeSize() / (1024 * 1024);;
	snprintf(Log_Buffer, Log_BufferLength, "%s: %llu MB / %llu MB", (char *) FPSTR(sdInfo), cardSize, freeSize);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
}


// Check if file-type is correct
bool fileValid(const char *_fileItem) {
	const char ch = '/';
	char *subst;
	subst = strrchr(_fileItem, ch); // Don't use files that start with .

	return (!startsWith(subst, (char *) "/.")) && (
			endsWith(_fileItem, ".mp3") || endsWith(_fileItem, ".MP3") ||
			endsWith(_fileItem, ".aac") || endsWith(_fileItem, ".AAC") ||
			endsWith(_fileItem, ".m3u") || endsWith(_fileItem, ".M3U") ||
			endsWith(_fileItem, ".m4a") || endsWith(_fileItem, ".M4A") ||
			endsWith(_fileItem, ".wav") || endsWith(_fileItem, ".WAV") ||
			endsWith(_fileItem, ".flac") || endsWith(_fileItem, ".FLAC") ||
			endsWith(_fileItem, ".asx") || endsWith(_fileItem, ".ASX"));
}


// Takes a directory as input and returns a random subdirectory from it
char *SdCard_pickRandomSubdirectory(char *_directory) {
	// Look if file/folder requested really exists. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory) {
		Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
		return NULL;
	}
	snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(tryToPickRandomDir), _directory);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	static uint8_t allocCount = 1;
	uint16_t allocSize = psramInit() ? 65535 : 1024;   // There's enough PSRAM. So we don't have to care...
	uint16_t directoryCount = 0;
	char *buffer = _directory;  // input char* is reused as it's content no longer needed
	char *subdirectoryList = (char *) x_calloc(allocSize, sizeof(char));

	if (subdirectoryList == NULL) {
		Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
		System_IndicateError();
		return NULL;
	}

	// Create linear list of subdirectories with #-delimiters
	while (true) {
		File fileItem = directory.openNextFile();
		if (!fileItem) {
			break;
		}
		if (!fileItem.isDirectory()) {
			continue;
		} else {
			const char* path = getPath(fileItem);

			/*snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(nameOfFileFound), buffer);
			Log_Println(Log_Buffer, LOGLEVEL_INFO);*/
			if ((strlen(subdirectoryList) + strlen(path) + 2) >= allocCount * allocSize) {
				char *tmp = (char *) realloc(subdirectoryList, ++allocCount * allocSize);
				Log_Println((char *) FPSTR(reallocCalled), LOGLEVEL_DEBUG);
				if (tmp == NULL) {
					Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
					System_IndicateError();
					free(subdirectoryList);
					return NULL;
				}
				subdirectoryList = tmp;
			}
			strcat(subdirectoryList, stringDelimiter);
			strcat(subdirectoryList, path);
			directoryCount++;
		}
	}
	strcat(subdirectoryList, stringDelimiter);

	if (!directoryCount) {
		free(subdirectoryList);
		return NULL;
	}

	uint16_t randomNumber = random(directoryCount) + 1;     // Create random-number with max = subdirectory-count
	uint16_t delimiterFoundCount = 0;
	uint32_t a=0;
	uint8_t b=0;

	// Walk through subdirectory-array and extract randomized subdirectory
	while (subdirectoryList[a] != '\0') {
		if (subdirectoryList[a] == '#') {
			delimiterFoundCount++;
		} else {
			if (delimiterFoundCount == randomNumber) {  // Pick subdirectory of linear char* according to random number
				buffer[b++] = subdirectoryList[a];
			}
		}
		if (delimiterFoundCount > randomNumber || (b == 254)) {  // It's over when next delimiter is found or buffer is full
			buffer[b] = '\0';
			free(subdirectoryList);
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(pickedRandomDir), _directory);
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			return buffer;  // Full path of random subdirectory
		}
		a++;
	}

	free(subdirectoryList);
	return NULL;
}


/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
Playlist *SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
	bool rebuildCacheFile = false;
	
	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
		return nullptr;
	}

	// Create linear playlist of caching-file
	#ifdef CACHED_PLAYLIST_ENABLE
		auto cacheFilePath = CacheFilePlaylist::getCachefilePath(fileOrDirectory);
		// Build absolute path of cacheFile


		// Decide if to use cacheFile. It needs to exist first check if cacheFile (already) exists
		// ...and playmode has to be != random/single (as random along with caching doesn't make sense at all)
		if (cacheFilePath && gFSystem.exists(cacheFilePath.value()) && _playMode != SINGLE_TRACK && _playMode != SINGLE_TRACK_LOOP) {
			// Read linear playlist (csv with #-delimiter) from cachefile (faster!)

			File cacheFile = gFSystem.open(cacheFilePath.value());
			if (cacheFile && cacheFile.size()) {
				CacheFilePlaylist *cachePlaylist = new CacheFilePlaylist();
				
				bool success = cachePlaylist->deserialize(cacheFile);
				if(success) {
					// always first assume a current playlist format
					return cachePlaylist;
				} else if(CacheFilePlaylist::isOldPlaylist(cacheFile)) {
					// read he old format and rewrite it into the new one
					cachePlaylist->deserializeOldPlaylist(cacheFile);
					cacheFile.close();

					// reopen for writing
					cacheFile = gFSystem.open(cacheFilePath.value(), FILE_WRITE);
					CacheFilePlaylist::serialize(cacheFile, *cachePlaylist);
					cacheFile.close();
					return cachePlaylist;
				}
				// we had some error reading the cache file, wait for the other to rebuild it
				// we do not need the class anymore, so destroy it
				delete cachePlaylist;
			}
			// we failed to read the cache file... set the flag to rebuild it
			rebuildCacheFile = true;
		}
	#endif

	snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeMemory), ESP.getFreeHeap());
	Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U) {
		Playlist *playlist = nullptr;
		if (fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size()) {
			// create a m3u plalist and parse the file

			// currently not implemented, so just drop this branch
			return nullptr;

		}
		return playlist;
	}

	// If we reached here, we did not read a cache file nor an m3u file. Means: read filenames from SD and make playlist of it
	Log_Println((char *) FPSTR(playlistGenModeUncached), LOGLEVEL_NOTICE);

	// File-mode
	if (!fileOrDirectory.isDirectory()) {
		Log_Println((char *) FPSTR(fileModeDetected), LOGLEVEL_INFO);
		const char *path = getPath(fileOrDirectory);
		if (fileValid(path)) {
			return new WebstreamPlaylist(path);
		}
	}

	// Folder mode
	FolderPlaylist *playlist = new FolderPlaylist();
	playlist->createFromFolder(fileOrDirectory);
	if(!playlist->isValid()) {
		// something went wrong
		Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
		delete playlist;
		return nullptr;
	}

	if(cacheFilePath && rebuildCacheFile) {
		File cacheFile = gFSystem.open(cacheFilePath.value(), FILE_WRITE);
		if(cacheFile) {
			CacheFilePlaylist::serialize(cacheFile, *playlist);
		}
		cacheFile.close();
	}

	snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(numberOfValidFiles), playlist->size());
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	// we are finished
	return playlist;
}
