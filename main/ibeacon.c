#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_log.h"

static const char* TAG = "IBEACON";

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))

#define IBEACON_UUID    {0xFD, 0xA5, 0x06, 0x93, 0xA4, 0xE2, 0x4F, 0xB1, \
                         0xAF, 0xCF, 0xC6, 0xEB, 0x07, 0x64, 0x78, 0x25}

#define IBEACON_MAJOR   10167
#define IBEACON_MINOR   61958

#define IBEACON_MEASURED_POWER  0xC5  // -59 dBm

typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;        // 0x004C = Apple
    uint16_t beacon_type;       // 0x1502 = iBeacon
} __attribute__((packed)) ibeacon_head_t;

typedef struct {
    uint8_t proximity_uuid[16];
    uint16_t major;
    uint16_t minor;
    int8_t measured_power;
} __attribute__((packed)) ibeacon_vendor_t;

typedef struct {
    ibeacon_head_t ibeacon_head;
    ibeacon_vendor_t ibeacon_vendor;
} __attribute__((packed)) ibeacon_packet_t;

static const ibeacon_head_t ibeacon_common_head = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x004C,
    .beacon_type = 0x1502
};

static const ibeacon_vendor_t vendor_config = {
    .proximity_uuid = IBEACON_UUID,
    .major = ENDIAN_CHANGE_U16(IBEACON_MAJOR),
    .minor = ENDIAN_CHANGE_U16(IBEACON_MINOR),
    .measured_power = IBEACON_MEASURED_POWER
};

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,                     // 20ms (0x20 * 0.625ms)
    .adv_int_max        = 0x40,                     // 40ms (0x40 * 0.625ms)
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "iBeacon setup complete, start advertising...");
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Error while starting advertising: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "iBeacon succesfully started!");
            ESP_LOGI(TAG, "UUID: %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                     vendor_config.proximity_uuid[0], vendor_config.proximity_uuid[1],
                     vendor_config.proximity_uuid[2], vendor_config.proximity_uuid[3],
                     vendor_config.proximity_uuid[4], vendor_config.proximity_uuid[5],
                     vendor_config.proximity_uuid[6], vendor_config.proximity_uuid[7],
                     vendor_config.proximity_uuid[8], vendor_config.proximity_uuid[9],
                     vendor_config.proximity_uuid[10], vendor_config.proximity_uuid[11],
                     vendor_config.proximity_uuid[12], vendor_config.proximity_uuid[13],
                     vendor_config.proximity_uuid[14], vendor_config.proximity_uuid[15]);
            ESP_LOGI(TAG, "Major: %d, Minor: %d", IBEACON_MAJOR, IBEACON_MINOR);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Error while stopping advertising: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Advertising succesfully stopped");
        }
        break;

    default:
        break;
    }
}

static esp_err_t create_ibeacon_data(ibeacon_packet_t *ibeacon_packet)
{
    if (ibeacon_packet == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&ibeacon_packet->ibeacon_head, &ibeacon_common_head, sizeof(ibeacon_head_t));

    memcpy(&ibeacon_packet->ibeacon_vendor, &vendor_config, sizeof(ibeacon_vendor_t));

    return ESP_OK;
}

static void start_ibeacon(void)
{
    esp_err_t status;

    ESP_LOGI(TAG, "Starting iBeacon...");

    status = esp_ble_gap_register_callback(gap_callback);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(status));
        return;
    }

    ibeacon_packet_t ibeacon_adv_data;
    status = create_ibeacon_data(&ibeacon_adv_data);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create iBeacon data: %s", esp_err_to_name(status));
        return;
    }

    status = esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure advertising data: %s", esp_err_to_name(status));
        return;
    }

    ESP_LOGI(TAG, "iBeacon ready, waiting for trasmission to start...");
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Starting iBeacon Sender ESP32");
    ESP_LOGI(TAG, "========================================");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Failed to start BT controller: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Failed to start BT controller: %s", esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Failed to start Bluedroid: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Failed to start Bluedroid: %s", esp_err_to_name(ret));
        return;
    }

    start_ibeacon();

    ESP_LOGI(TAG, "iBeacon running. The device is now trasmitting.");
}
