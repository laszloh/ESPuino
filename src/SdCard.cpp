#include <Arduino.h>
#include "settings.h"
#include "SdCard.h"
#include "Common.h"
#include "Led.h"
#include "Log.h"
#include "MemX.h"
#include "System.h"

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
bool SdCard_pickRandomSubdirectory(char *_directory) {
	// Look if file/folder requested really exists. If not => break.
	File directory = gFSystem.open(_directory);
	if (!directory) {
		Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
		return false;
	}
	snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(tryToPickRandomDir), _directory);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	// count the number of subdir's
	size_t subDirCount = 0;
	while(true){
		File fileItem = directory.openNextFile();
		if(!fileItem) {
			break;
		}
		if(!fileItem.isDirectory()) {
			continue;
		}
		
		subDirCount++;
	}

	// no sub directories, return "meh"
	if(subDirCount == 0) {
		return false;
	}
	const size_t randomNumber = random(subDirCount) + 1;     // Create random-number with max = subdirectory-count

	constexpr size_t maxFileLenght = 255;
	// we reuse the input buffer (funtion should not return char*)
	char *buffer = _directory;	//static_cast<char*>(malloc(maxFileLenght));		// this is the max lenght of a FAT32 filename

	directory.rewindDirectory();
	size_t counter = 0;
	while(true) {
		File fileItem = directory.openNextFile();
		if(!fileItem) {
			break;
		}
		if(!fileItem.isDirectory()) {
			continue;
		}
		
		counter++;
		if(counter == randomNumber) {
			// we found our folder
#if ESP_ARDUINO_VERSION_MAJOR >= 2
			const char *path = fileItem.path();
#else
			const char *path = fileItem.name();
#endif
			strncpy(buffer, path, maxFileLenght);
			snprintf(Log_Buffer, Log_BufferLength, "%s: %s", (char *) FPSTR(pickedRandomDir), _directory);
			Log_Println(Log_Buffer, LOGLEVEL_NOTICE);
			return true;  // Full path of random subdirectory
		}
	}
	return false;
}


/* Puts SD-file(s) or directory into a playlist
	First element of array always contains the number of payload-items. */
const Playlist *SdCard_ReturnPlaylist(const char *fileName, const uint32_t _playMode) {
	static Playlist playlist;
	char cacheFileNameBuf[275];
	bool readFromCacheFile = false;
	bool enablePlaylistCaching = false;

	// Look if file/folder requested really exists. If not => break.
	File fileOrDirectory = gFSystem.open(fileName);
	if (!fileOrDirectory) {
		Log_Println((char *) FPSTR(dirOrFileDoesNotExist), LOGLEVEL_ERROR);
		return NULL;
	}

	snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeMemory), ESP.getFreeHeap());
	Log_Println(Log_Buffer, LOGLEVEL_DEBUG);

	// clean up previous data
	if (playlist.files != NULL) {
		Log_Println((char *) FPSTR(releaseMemoryOfOldPlaylist), LOGLEVEL_DEBUG);
		freeMultiCharArray(playlist.files, playlist.numFiles);
		playlist.numFiles = 0;
		snprintf(Log_Buffer, Log_BufferLength, "%s: %u", (char *) FPSTR(freeMemoryAfterFree), ESP.getFreeHeap());
		Log_Println(Log_Buffer, LOGLEVEL_DEBUG);
	}

	// Parse m3u-playlist and create linear-playlist out of it
	if (_playMode == LOCAL_M3U && fileOrDirectory && !fileOrDirectory.isDirectory() && fileOrDirectory.size() >= 0) {
		// count the number of lines int he m3u
		while(fileOrDirectory.available() > 0) {
			char b = fileOrDirectory.read();
			if(b == '\n') {
				playlist.numFiles++;
			}
		}
		fileOrDirectory.seek(0);

		playlist.files = static_cast<char**>(malloc(sizeof(char*) * playlist.numFiles));
		if(playlist.files == NULL) {
			// malloc failed
			Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
			System_IndicateError();
			return NULL;
		}

		// read file by line
		size_t cnt = 0;
		while(fileOrDirectory.available() > 0) {
			String line = fileOrDirectory.readStringUntil('\n');
			// ignore extended m3u lines
			if(line.startsWith("#"))
				continue;

			// strip newline characters (f.e. CR)
			line.trim();

			// and save it
			playlist.files[cnt] = x_strdup(line.c_str());
			cnt++;
		}

		// we are finished
		return &playlist;
	}

	// Create linear playlist of caching-file
	#ifdef CACHED_PLAYLIST_ENABLE
		snprintf(cacheFileNameBuf, sizeof(cacheFileNameBuf), "%s/%s", fileName, FPSTR(playlistCacheFile));

		// Decide if to use cacheFile. It needs to exist first...
		if (gFSystem.exists(cacheFileNameBuf)) {     // Check if cacheFile (already) exists
			readFromCacheFile = true;
		}

		// ...and playmode has to be != random/single (as random along with caching doesn't make sense at all)
		if (_playMode == SINGLE_TRACK ||
			_playMode == SINGLE_TRACK_LOOP) {
				readFromCacheFile = false;
		} else {
			enablePlaylistCaching = true;
		}

		// Read linear playlist (csv with #-delimiter) from cachefile (faster!)
		if (readFromCacheFile) {
			File cacheFile = gFSystem.open(cacheFileNameBuf);
			if (cacheFile) {
				uint32_t cacheFileSize = cacheFile.size();

				if (!cacheFileSize) {        // Make sure it's greater than 0 bytes
					Log_Println((char *) FPSTR(playlistCacheFoundBut0), LOGLEVEL_ERROR);
					cacheFile.close();
					gFSystem.remove(cacheFileNameBuf);
				} else {
					Log_Println((char *) FPSTR(playlistGenModeCached), LOGLEVEL_NOTICE);

					// assume new cache format
					const size_t lineCount = cacheFile.readStringUntil('#').toInt();
					if(!lineCount) {
						// old file, delete cache
						cacheFile.close();
						gFSystem.remove(cacheFileNameBuf);
					} else {
						// prepare playlist
						playlist.numFiles = lineCount;
						playlist.files = static_cast<char**>(malloc(sizeof(char*) * lineCount));
						if(playlist.files == NULL) {
							// malloc failed
							Log_Println((char *) FPSTR(unableToAllocateMemForLinearPlaylist), LOGLEVEL_ERROR);
							System_IndicateError();
							return NULL;
						}

						size_t cnt = 0;
						while(cacheFile.available() > 0){
							const String line = cacheFile.readStringUntil('#');
							playlist.files[cnt] = x_strdup(line.c_str());
							cnt++;
						}
						return &playlist;
					}
				}
			}
		}
	#endif

	// if we reach here, we didn't read from cachefile or m3u-file. Means: read filenames from SD and make playlist of it
	Log_Println((char *) FPSTR(playlistGenModeUncached), LOGLEVEL_NOTICE);

	// File-mode
	if (!fileOrDirectory.isDirectory()) {
		playlist.numFiles = 1;
		playlist.files = static_cast<char**>(malloc(sizeof(char*)));
		if (playlist.files == NULL) {
			Log_Println((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
			System_IndicateError();
			return NULL;
		}
		Log_Println((char *) FPSTR(fileModeDetected), LOGLEVEL_INFO);
		#if ESP_ARDUINO_VERSION_MAJOR >= 2
			const char *path = fileOrDirectory.path();
		#else
			const char *path = fileOrDirectory.name();
		#endif
		if (fileValid(path)) {
			playlist.files[0] = x_strdup(path);
		}
		return &playlist;
	}

	// Directory-mode (linear-playlist)
	// prepare playlist with a sane amount if entries
	size_t allocCount = 1;
	constexpr size_t allocSlots = 32;

	playlist.files = static_cast<char**>(malloc(sizeof(char*) * allocSlots * allocCount));
	if(playlist.files == NULL) {
		// we failed!
		Log_Println((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
		System_IndicateError();
		return NULL;
	}

	while(true) {
		// go through the directory and register all playable files
		File fileItem = fileOrDirectory.openNextFile();
		if(!fileItem) {
			break;
		}
		if(fileItem.isDirectory()){
			continue;
		}
		#if ESP_ARDUINO_VERSION_MAJOR >= 2
			const char *path = fileOrDirectory.path();
		#else
			const char *path = fileOrDirectory.name();
		#endif
		if(fileValid(path)) {
			playlist.files[playlist.numFiles] = x_strdup(path);
			playlist.numFiles++;
			// test if we have to increase the slot count
			if(playlist.numFiles == (allocSlots * allocCount)) {
				allocCount++;

				// use temporary buffer to retain playlist.files if we fail
				char **tmpBuffer = static_cast<char**>(realloc(playlist.files, sizeof(char*) * allocCount * allocSlots ));
				if(tmpBuffer == NULL) {
					// we failed!
					Log_Println((char *) FPSTR(unableToAllocateMemForPlaylist), LOGLEVEL_ERROR);
					System_IndicateError();
					return NULL;
				}
				playlist.files = tmpBuffer;
			}
		}
	}

	snprintf(Log_Buffer, Log_BufferLength, "%s: %d", (char *) FPSTR(numberOfValidFiles), playlist.numFiles);
	Log_Println(Log_Buffer, LOGLEVEL_NOTICE);

	// create playlist cache
	File cacheFile;
	if(enablePlaylistCaching) {
		cacheFile = gFSystem.open(cacheFileNameBuf, FILE_WRITE);
		if(cacheFile) {
			// write number of entries
			cacheFile.printf("%d#", playlist.numFiles);
			for(size_t i=0;i<playlist.numFiles;i++) {
				cacheFile.print(playlist.files[i]);
				cacheFile.print('#');
			}
			cacheFile.close();
		}
	}

	return &playlist;
}
