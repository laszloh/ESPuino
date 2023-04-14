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
std::optional<const String> SdCard_pickRandomSubdirectory(const char *_directory) {
	// Look if file/folder requested really exists. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory) {
		// does not exists
		Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
		return std::nullopt;
	}
	snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(tryToPickRandomDir), _directory);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	size_t dirCount = 0;
	while(true) {
		bool isDir;
		const String path = directory.getNextFileName(&isDir);
		if(!path) {
			break;
		}
		if(isDir) {
			dirCount++;
		}
	}
	if(!dirCount) {
		// no paths in folder
		return std::nullopt;
	}

	const uint32_t randomNumber = esp_random() % dirCount;
	String path;
	for(size_t i=0;i<randomNumber;) {
		bool isDir;
		path = directory.getNextFileName(&isDir);
		if(!path) {
			// we reached the end before finding the correct dir!
			return std::nullopt;
		}
		if(isDir) {
			i++;
		}
	}
	snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(pickedRandomDir), path.c_str());
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
	
	return path;
}

static std::optional<Playlist*> SdCard_ParseM3UPlaylist(File f, bool forceExtended = false) {
	const String line = f.readStringUntil('\n');
	bool extended = line.startsWith("#EXTM3U") || forceExtended;
	FolderPlaylist *playlist = new FolderPlaylist();

	if(extended) {
        // extended m3u file format
        // ignore all lines starting with '#'

        while(f.available()) {
            String line = f.readStringUntil('\n');
            if(!line.startsWith("#")){
                // this something we have to save
                line.trim(); 
                if(!playlist->push_back(line)) {
					delete playlist;
                    return std::nullopt;
                }
            }
        }
        // resize memory to fit our count
		playlist->compress();
        return playlist;
	}

	// normal m3u is just a bunch of filenames, 1 / line
	f.seek(0);
	while(f.available()) {
		String line = f.readStringUntil('\n');
		line.trim();
		if(!playlist->push_back(line)) {
			delete playlist;
			return std::nullopt;
		}
	}
	// resize memory to fit our count
	playlist->compress();
	return playlist;
}

/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
std::optional<Playlist*> SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
	bool rebuildCacheFile = false;
	
	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
		return std::nullopt;
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
		if (fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size()) {
			// create a m3u playlist and parse the file
			return SdCard_ParseM3UPlaylist(fileOrDirectory);
		}
		// if we reach here, we failed
		return std::nullopt;
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
		return std::nullopt;
	}

	#ifdef CACHED_PLAYLIST_ENABLE
		if(cacheFilePath && rebuildCacheFile) {
			File cacheFile = gFSystem.open(cacheFilePath.value(), FILE_WRITE);
			if(cacheFile) {
				CacheFilePlaylist::serialize(cacheFile, *playlist);
			}
			cacheFile.close();
		}
	#endif

	snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(numberOfValidFiles), playlist->size());
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	// we are finished
	return playlist;
}
