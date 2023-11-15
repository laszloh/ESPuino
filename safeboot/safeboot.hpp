#pragma once

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#include <stdint.h>
#include <string.h>

namespace safeboot {

constexpr const char *firmwarePath = "/firmware.bin";

enum class Partition : uint8_t {
	FACTORY = 0,
	APP,
	TOGGLE
};

bool isRunningFactoryPartition(void) {
	const esp_partition_t *cur_part = esp_ota_get_running_partition();
	return strcmp(cur_part->label, "safeboot") == 0;
}

void restartToSafeBoot() {
	const esp_partition_t *otadata_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
	if (otadata_partition) {
		auto ret = esp_partition_erase_range(otadata_partition, 0, SPI_FLASH_SEC_SIZE * 2);
		esp_restart();
	}
}

const esp_partition_t *getApplicationPartiton() {
	return esp_ota_get_next_update_partition(nullptr);
}

void restartToApplication() {
	auto partition = getApplicationPartiton();
	auto ret = esp_ota_set_boot_partition(partition);
	esp_restart();
}

void switchPartition(Partition state) {
	bool runningFactory = isRunningFactoryPartition();
	switch (state) {
		case Partition::FACTORY:
			if (!runningFactory) {
				restartToSafeBoot();
			}
			break;

		case Partition::APP:
			if (runningFactory) {
				restartToApplication();
			}
			break;

		case Partition::TOGGLE:
			if (runningFactory) {
				restartToApplication();
			} else {
				restartToSafeBoot();
			}
			break;
	}
}

} // namespace safeboot
