#include <Arduino.h>
#include "../src/settings.h" // Contains all user-relevant settings (general)

#include "safeboot.hpp"

#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <esp_partition.h>

static constexpr const char *logo = R"literal(
 _____   ____    ____            _
| ____| / ___|  |  _ \   _   _  (_)  _ __     ___
|  _|   \__  \  | |_) | | | | | | | | '_ \   / _ \
| |___   ___) | |  __/  | |_| | | | | | | | | (_) |
|_____| |____/  |_|      \__,_| |_| |_| |_|  \___/
         Rfid-controlled musicplayer


)literal";

#if defined(SD_MMC_1BIT_MODE)
fs::FS sdCard = (fs::FS) SD_MMC;
#elif defined(SINGLE_SPI_ENABLE)
SPIClass spiSD(HSPI);
fs::FS sdCard = (fs::FS) SD;
#else
	#error safeboot currently only works with SD Card
#endif

constexpr size_t FLASH_BLOCK_SIZE = 4 * 4096;
uint8_t buffer[FLASH_BLOCK_SIZE];
bool inRecovery = false;

bool flashPartition(File &f);
void prepRecoveryCycle();

inline bool mountSdCard() {
#ifdef SD_MMC_1BIT_MODE
	// pinMode(2, INPUT_PULLUP);
	return SD_MMC.begin("/sdcard", true);
#else
	spiSD.begin(SPISD_SCK, SPISD_MISO, SPISD_MOSI, SPISD_CS);
	spiSD.setFrequency(1000000);
	return SD.begin(SPISD_CS, spiSD);
#endif
}

#ifdef INVERT_POWER
constexpr bool inverted = true;
#else
constexpr bool inverted = false;
#endif

void setup() {
	constexpr size_t maxRetries = 10;

	Serial.begin(115200);
	Serial.print(logo);
	srand(esp_random());

	pinMode(POWER, OUTPUT);
	digitalWrite(POWER, (inverted) ? LOW : HIGH);

	log_i("Booting firmware safeboot OTA system");

	log_i("Mounting SDCard");
	size_t retries = 0;
	while (!mountSdCard()) {
		retries++;
		log_e("Mount failed... (%d of %d tries)", retries, maxRetries);
		delay(100);
		if (retries > maxRetries) {
			// failed to mount card
			log_e("Could not mount card. Aborting OTA!");
			safeboot::restartToApplication();
		}
	}

	// sdcard is here, check if we have to do a recovery
	if (sdCard.exists(safeboot::recoveryPath)) {
		File fw = sdCard.open(safeboot::recoveryPath, FILE_READ);

		log_i("Found recovery image. Starting recovery progress...");
		inRecovery = true;

		// we do not make a backup
		if (flashPartition(fw)) {
			log_i("recovery finished. Rebooting to application");
			sdCard.rename(safeboot::recoveryPath, safeboot::backupPath);
			safeboot::restartToApplication();
		} else {
			log_e("Recovery failed! If this persists, please reflash with VSCode to recover");
			log_e("Rebooting in 5s...");
			delay(5000);
			safeboot::restartToSafeBoot();
		}
	}

	// we do not have to do a recovery, check if we have a firmware.bin
	if (!sdCard.exists(safeboot::firmwarePath)) {
		// nothing to do
		log_i("No firmware found at \"%s\". rebooting", safeboot::firmwarePath);
		safeboot::restartToApplication();
	}

	log_i("Found firmware file");
	File fw = sdCard.open(safeboot::firmwarePath, FILE_READ);

	// create a backup
	log_i("Creating backup at \"%s\"", safeboot::backupPath);
	auto partition = safeboot::getApplicationPartiton();
	File backup = sdCard.open(safeboot::backupPath, FILE_WRITE);
	for (size_t idx = 0; idx < partition->size; idx += FLASH_BLOCK_SIZE) {
		Serial.print('.');
		esp_partition_read(partition, idx, buffer, FLASH_BLOCK_SIZE);
		backup.write(buffer, FLASH_BLOCK_SIZE);
	}
	Serial.println();
	log_i("Backup file written");
	backup.close();

	// execute the update
	if (flashPartition(fw)) {
		log_i("OTA finished, rebooting to application");
		sdCard.remove(safeboot::firmwarePath);
	} else {
		log_e("OTA failed before flash access, rebooting to application");
	}
	safeboot::restartToApplication();
}

void loop() {
	// we should never get here, so just reboot to the application
	log_e("loop called, rebooting!");
	safeboot::restartToApplication();
}

void prepRecoveryCycle() {
	if (!inRecovery) {
		log_e("Application image propably damaged, startign recovery...");
		sdCard.rename(safeboot::backupPath, safeboot::recoveryPath);
	}
	esp_restart();
}

bool flashPartition(File &f) {
	const size_t fwSize = f.size();
	auto partition = safeboot::getApplicationPartiton();
	if (fwSize >= partition->size) {
		// this won't fit
		log_e("Firmware too big to fit into partition (got %d bytes, have %d).", fwSize, partition->size);
		return false;
	}

	esp_ota_handle_t handle;
	auto ret = esp_ota_begin(partition, fwSize, &handle);
	if (ret != ESP_OK) {
		log_e("failed to start OTA: %d", ret);
		return false;
	}
	while (f.available()) {
		const size_t len = f.read(buffer, FLASH_BLOCK_SIZE);
		Serial.print('.');

		ret = esp_ota_write(handle, buffer, len);
		if (ret != ESP_OK) {
			log_e("Failed to write image: %d", ret);
			prepRecoveryCycle();
		}
	}
	Serial.println();
	f.close();
	ret = esp_ota_end(handle);
	if (ret != ESP_OK) {
		log_e("Failed to finish OTA: %d", ret);
		prepRecoveryCycle();
	}
	return true;
}
