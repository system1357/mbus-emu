/*
 * Written by William Peters in 2021
 */

// Platform generic includes
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// application includes
#include "include/mbus.h"
#include "include/bt.h"

// setup functions.
void app_setup(void) {
  ESP_LOGI("SETUP", "Initializing...");
  gpio_install_isr_service(ESP_INTR_FLAG_SHARED); //enable shared interrupts
  mbus_setup();
  bt_setup();
}

// entry point
void app_main(void) {
    ESP_LOGI("Main", "Booting...");
    app_setup();

    for( ;; ) { // main loop, as we use tasks, timers and interrupts, this should stay clean.
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}