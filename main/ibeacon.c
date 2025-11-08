#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_system.h"
#include "esp_vfs_dev.h"
#include "esp_log.h"

static const char* TAG = "IBEACON";

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))

#define IBEACON_UUID    {0xFD, 0xA5, 0x06, 0x93, 0xA4, 0xE2, 0x4F, 0xB1, \
                         0xAF, 0xCF, 0xC6, 0xEB, 0x07, 0x64, 0x78, 0x25}

#define IBEACON_MAJOR_DEFAULT   10167
#define IBEACON_MINOR_DEFAULT   61958

#define IBEACON_MEASURED_POWER  0xC5  // -59 dBm

#define CLI_TASK_STACK_SIZE      4096
#define CLI_TASK_PRIORITY        5
#define CLI_INPUT_MAX_LEN        64
#define IBEACON_LED_GPIO         GPIO_NUM_2

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

static ibeacon_vendor_t vendor_config = {
    .proximity_uuid = IBEACON_UUID,
    .major = ENDIAN_CHANGE_U16(IBEACON_MAJOR_DEFAULT),
    .minor = ENDIAN_CHANGE_U16(IBEACON_MINOR_DEFAULT),
    .measured_power = IBEACON_MEASURED_POWER
};

typedef struct {
    uint16_t major;
    uint16_t minor;
} beacon_runtime_config_t;

static beacon_runtime_config_t beacon_config = {
    .major = IBEACON_MAJOR_DEFAULT,
    .minor = IBEACON_MINOR_DEFAULT
};

static volatile bool s_advertising_active = false;

static void print_cli_help(void);
static void blink_status_led(uint32_t duration_seconds);
static void handle_cli_command(char *line);

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,                     // 20ms (0x20 * 0.625ms)
    .adv_int_max        = 0x40,                     // 40ms (0x40 * 0.625ms)
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_err_t create_ibeacon_data(ibeacon_packet_t *ibeacon_packet);

static void sync_vendor_config(void)
{
    vendor_config.major = ENDIAN_CHANGE_U16(beacon_config.major);
    vendor_config.minor = ENDIAN_CHANGE_U16(beacon_config.minor);
}

static void load_beacon_config_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("ibeacon", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return;
    }

    uint16_t value = 0;

    err = nvs_get_u16(handle, "major", &value);
    if (err == ESP_OK) {
        beacon_config.major = value;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Major not found in NVS. Using default value %u", beacon_config.major);
    } else {
        ESP_LOGE(TAG, "Error reading major from NVS: %s", esp_err_to_name(err));
    }

    err = nvs_get_u16(handle, "minor", &value);
    if (err == ESP_OK) {
        beacon_config.minor = value;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Minor not found in NVS. Using default value %u", beacon_config.minor);
    } else {
        ESP_LOGE(TAG, "Error reading minor from NVS: %s", esp_err_to_name(err));
    }

    nvs_close(handle);

    sync_vendor_config();

    ESP_LOGI(TAG, "Loaded beacon config - Major: %u, Minor: %u", beacon_config.major, beacon_config.minor);
}

static esp_err_t save_beacon_config_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("ibeacon", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u16(handle, "major", beacon_config.major);
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, "minor", beacon_config.minor);
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved beacon config - Major: %u, Minor: %u", beacon_config.major, beacon_config.minor);
    } else {
        ESP_LOGE(TAG, "Failed to save beacon config: %s", esp_err_to_name(err));
    }

    return err;
}

static esp_err_t update_advertising_payload(bool stop_first)
{
    ibeacon_packet_t ibeacon_adv_data;
    esp_err_t status;

    if (stop_first && s_advertising_active) {
        status = esp_ble_gap_stop_advertising();
        if (status != ESP_OK && status != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to stop advertising: %s", esp_err_to_name(status));
            return status;
        }

        if (status == ESP_OK) {
            int attempts = 0;
            while (s_advertising_active && attempts < 20) {
                vTaskDelay(pdMS_TO_TICKS(50));
                attempts++;
            }

            if (s_advertising_active) {
                ESP_LOGW(TAG, "Advertising stop timed out; continuing with update");
            }
        }
    }

    sync_vendor_config();
    status = create_ibeacon_data(&ibeacon_adv_data);
    if (status != ESP_OK) {
        return status;
    }

    status = esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure advertising data: %s", esp_err_to_name(status));
    }

    return status;
}

static void init_console_uart(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static void handle_cli_command(char *line)
{
    char *saveptr = NULL;
    char *token = strtok_r(line, " \t\r\n", &saveptr);
    if (token == NULL) {
        return;
    }

    if (strcmp(token, "help") == 0) {
        print_cli_help();
        return;
    }

    if (strcmp(token, "show") == 0) {
        printf("UUID : %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
               vendor_config.proximity_uuid[0], vendor_config.proximity_uuid[1],
               vendor_config.proximity_uuid[2], vendor_config.proximity_uuid[3],
               vendor_config.proximity_uuid[4], vendor_config.proximity_uuid[5],
               vendor_config.proximity_uuid[6], vendor_config.proximity_uuid[7],
               vendor_config.proximity_uuid[8], vendor_config.proximity_uuid[9],
               vendor_config.proximity_uuid[10], vendor_config.proximity_uuid[11],
               vendor_config.proximity_uuid[12], vendor_config.proximity_uuid[13],
               vendor_config.proximity_uuid[14], vendor_config.proximity_uuid[15]);
        printf("Major: %u\n", beacon_config.major);
        printf("Minor: %u\n", beacon_config.minor);
        return;
    }

    if (strcmp(token, "set_major") == 0) {
        char *value_str = strtok_r(NULL, " \t\r\n", &saveptr);
        if (value_str == NULL) {
            printf("Usage: set_major <value>\n");
            return;
        }

        long value = strtol(value_str, NULL, 10);
        if (value < 0 || value > UINT16_MAX) {
            printf("Value out of range (0-65535).\n");
            return;
        }

        beacon_config.major = (uint16_t)value;
        printf("Major value staged: %u\n", beacon_config.major);
        return;
    }

    if (strcmp(token, "set_minor") == 0) {
        char *value_str = strtok_r(NULL, " \t\r\n", &saveptr);
        if (value_str == NULL) {
            printf("Usage: set_minor <value>\n");
            return;
        }

        long value = strtol(value_str, NULL, 10);
        if (value < 0 || value > UINT16_MAX) {
            printf("Value out of range (0-65535).\n");
            return;
        }

        beacon_config.minor = (uint16_t)value;
        printf("Minor value staged: %u\n", beacon_config.minor);
        return;
    }

    if (strcmp(token, "save") == 0) {
        if (save_beacon_config_to_nvs() == ESP_OK) {
            if (update_advertising_payload(true) == ESP_OK) {
                printf("Configuration saved and advertising updated.\n");
            } else {
                printf("Configuration saved but failed to update advertising.\n");
            }
        }
        return;
    }

    if (strcmp(token, "blink") == 0) {
        char *value_str = strtok_r(NULL, " \t\r\n", &saveptr);
        uint32_t seconds = 3;
        if (value_str != NULL) {
            long value = strtol(value_str, NULL, 10);
            if (value > 0) {
                seconds = (uint32_t)value;
            }
        }
        blink_status_led(seconds);
        return;
    }

    if (strcmp(token, "restart") == 0) {
        printf("Restarting...\n");
        fflush(stdout);
        esp_restart();
    }

    printf("Unknown command. Type 'help' for the command list.\n");
}

static void init_status_led(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << IBEACON_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure status LED: %s", esp_err_to_name(err));
    }

    gpio_set_level(IBEACON_LED_GPIO, 0);
}

static void blink_status_led(uint32_t duration_seconds)
{
    if (duration_seconds == 0) {
        duration_seconds = 1;
    }

    const TickType_t half_period = pdMS_TO_TICKS(250);
    TickType_t remaining = pdMS_TO_TICKS(duration_seconds * 1000);

    while (remaining > 0) {
        gpio_set_level(IBEACON_LED_GPIO, 1);
        vTaskDelay(half_period);
        gpio_set_level(IBEACON_LED_GPIO, 0);
        vTaskDelay(half_period);

        if (remaining > 2 * half_period) {
            remaining -= 2 * half_period;
        } else {
            remaining = 0;
        }
    }
}

static void print_cli_help(void)
{
    printf("Available commands:\n");
    printf("  help                Show this message\n");
    printf("  show                Display current UUID/Major/Minor\n");
    printf("  set_major <value>   Set major (0-65535)\n");
    printf("  set_minor <value>   Set minor (0-65535)\n");
    printf("  save                Persist values and update advertising\n");
    printf("  blink <seconds>     Blink status LED to locate device\n");
    printf("  restart             Restart device\n");
}

static void cli_task(void *arg)
{
    char line[CLI_INPUT_MAX_LEN];
    size_t index = 0;
    bool last_was_cr = false;
    const char *prompt = "\r\nibeacon> ";

    printf("%s", prompt);
    fflush(stdout);

    while (true) {
        uint8_t ch;
        int len = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &ch, 1, portMAX_DELAY);
        if (len <= 0) {
            continue;
        }

        if (ch == '\n') {
            if (last_was_cr) {
                last_was_cr = false;
                continue;
            }
        } else {
            last_was_cr = false;
        }

        if (ch == '\r' || ch == '\n') {
            line[index] = '\0';
            printf("\r\n");
            if (index > 0) {
                handle_cli_command(line);
            }
            index = 0;
            printf("ibeacon> ");
            fflush(stdout);
            if (ch == '\r') {
                last_was_cr = true;
            }
            continue;
        }

        if (ch == 0x7F || ch == '\b') {
            if (index > 0) {
                index--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (index < CLI_INPUT_MAX_LEN - 1 && ch >= 0x20 && ch <= 0x7E) {
            line[index++] = (char)ch;
            putchar(ch);
            fflush(stdout);
        }
    }
}
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
            s_advertising_active = true;
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
            ESP_LOGI(TAG, "Major: %u, Minor: %u", beacon_config.major, beacon_config.minor);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Error while stopping advertising: %s", esp_err_to_name(err));
        } else {
            s_advertising_active = false;
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

    status = update_advertising_payload(false);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure advertising data: %s", esp_err_to_name(status));
        return;
    }

    ESP_LOGI(TAG, "iBeacon ready, waiting for trasmission to start...");
}

void app_main(void)
{
    esp_err_t ret;

    init_console_uart();

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Starting iBeacon Sender ESP32");
    ESP_LOGI(TAG, "========================================");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_beacon_config_from_nvs();

    init_status_led();

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

    print_cli_help();

    if (xTaskCreate(cli_task, "ibeacon_cli", CLI_TASK_STACK_SIZE, NULL, CLI_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start CLI task");
    }
}
