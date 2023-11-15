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

void setup() {
	constexpr size_t maxRetries = 10;
	size_t retries = 0;

	Serial.begin(115200);
	Serial.print(logo);

	log_i("Booting firmware safeboot OTA system");

	const auto mountSD = []() -> bool {
#ifdef SD_MMC_1BIT_MODE
		pinMode(2, INPUT_PULLUP);
		return SD_MMC.begin("/sdcard", true);
#else
		spiSD.begin(SPISD_SCK, SPISD_MISO, SPISD_MOSI, SPISD_CS);
		spiSD.setFrequency(1000000);
		return SD.begin(SPISD_CS, spiSD);
#endif
	};

	log_i("Mounting SDCard in MMC mode");
	while (!mountSD()) {
		retries++;
		log_e("Mount failed... (%d of %d tries)", retries, maxRetries);
		delay(100);
		if (retries > maxRetries) {
			// failed to mount card
			log_e("Could not mount card. Aborting OTA!");
			safeboot::restartToApplication();
		}
	}

	// card avaliable, check if we have a firmware.bin
	if (!sdCard.exists(safeboot::firmwarePath)) {
		// nothing to do
		log_i("No firmware found at \"%s\". rebooting", safeboot::firmwarePath);
		safeboot::restartToApplication();
	}

	File fw = sdCard.open(safeboot::firmwarePath);
	if (!fw) {
		log_e("Could not open file!");
		safeboot::restartToApplication();
	}

	// some basic check, so that we do not break the ESP
	const size_t fwSize = fw.size();
	auto partition = safeboot::getApplicationPartiton();
	if (fwSize >= partition->size) {
		// this won't fit
		log_e("Firmware too big to fit into partition (got %d bytes, have %d).", fwSize, partition->size);
		safeboot::restartToApplication();
	}

	// no way back from here on
	log_i("Found new firmware. Starting upgrade progress...");

	esp_ota_handle_t handle;
	auto ret = esp_ota_begin(partition, fwSize, &handle);
	if (ret != ESP_OK) {
		log_e("failed to start OTA: %d", ret);
		safeboot::restartToApplication();
	}
	while (fw.available()) {
		static uint8_t buf[4096]; // put the buffer on the data segment instead of the stack

		const size_t len = fw.read(buf, sizeof(buf));
		log_i("Writing %d out of %d", fw.position(), fwSize);

		ret = esp_ota_write(handle, buf, len);
		if (ret != ESP_OK) {
			log_e("Failed to write image: %d", ret);
			log_e("Application image propably damaged, rebooting OTA system");
			esp_restart();
		}
	}
	ret = esp_ota_end(handle);
	if (ret != ESP_OK) {
		log_e("Failed to finish OTA: %d", ret);
		log_e("Application image propably damaged, rebooting OTA system");
		esp_restart();
	}
	log_i("OTA finished, rebooting to application");
	safeboot::restartToApplication();
}

void loop() {
	// we should never get here, so just reboot to the application
	log_e("loop called, rebooting!");
	safeboot::restartToApplication();
}
