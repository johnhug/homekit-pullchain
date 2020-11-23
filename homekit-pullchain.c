/*
* HomeKit interface for shared control of a pullchain lamp
* 
*/
#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#define LED_INBUILT_GPIO    2
#define PULLCHAIN_GPIO      12 // D6
#define RELAY_GPIO          4 // D2

#define PULLCHAIN_PAUSE     500

uint32_t last_interrupt = 0;

// Home Kit variables
bool hk_on = false;

void identify_task(void *_args) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 9; j++) {
            gpio_write(LED_INBUILT_GPIO, 0);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_write(LED_INBUILT_GPIO, 1);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void updateRelay() {
    printf("updateRelay: %d\n", hk_on);
    gpio_write(RELAY_GPIO, hk_on ? 1 : 0);
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t on_get() {
    return HOMEKIT_BOOL(hk_on);
}

void on_set(homekit_value_t value) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (last_interrupt < now - PULLCHAIN_PAUSE) {
        last_interrupt = now;
        hk_on = value.bool_value;
        updateRelay();
    }
}

homekit_characteristic_t on = HOMEKIT_CHARACTERISTIC_(
    ON, false,
    .getter = on_get,
    .setter = on_set,
    );

void gpio_intr_handler(uint8_t gpio_num) {
    uint32_t now = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    if (last_interrupt < now - PULLCHAIN_PAUSE) {
        last_interrupt = now;
        printf("pullthechain: %d\n", gpio_read(PULLCHAIN_GPIO));
        hk_on = !hk_on;
        homekit_characteristic_notify(&on, HOMEKIT_BOOL(hk_on));
        updateRelay();
    }
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Chouchin");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id = 1,
        .category = homekit_accessory_category_lightbulb,
        .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "John Hug"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "c7ec465bf5e9"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Årstid-Assist"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.5"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Chouchin"),
            &on,
            NULL
        }),
        NULL
    }),
    NULL
};

void on_homekit_event(homekit_event_t event) {
    if (event == HOMEKIT_EVENT_CLIENT_VERIFIED) {
        identify(HOMEKIT_INT(1));    
    }
}

homekit_server_config_t config = {
    .accessories = accessories,
    .category = homekit_accessory_category_lightbulb,
    .password = "720-27-703",
    .setupId = "8A5C",
    .on_event = on_homekit_event
};

void on_wifi_ready() {
    identify(HOMEKIT_INT(1));    
}

void user_init(void) {
    gpio_enable(LED_INBUILT_GPIO, GPIO_OUTPUT);
    gpio_enable(RELAY_GPIO, GPIO_OUTPUT);
    gpio_enable(PULLCHAIN_GPIO, GPIO_INPUT);
    gpio_set_interrupt(PULLCHAIN_GPIO, GPIO_INTTYPE_EDGE_ANY, gpio_intr_handler);

    wifi_config_init("Chouchin", NULL, on_wifi_ready);

    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    int name_len = 8 + 1 + 6 + 1;
    char *name_value = malloc(name_len);
    snprintf(name_value, name_len, "Chouchin-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
 
    homekit_server_init(&config);
}
