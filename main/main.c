/*
 * Written by William Peters in 2021
 */

// Platform generic includes
#include <stdio.h>
#include "sdkconfig.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// application includes
#include "include/mbus.h"
#include "include/network.h"
#include "include/io.h"

// setup functions.
void app_setup(void) {
  printf("[SETUP] Initializing...\n");
  gpio_install_isr_service(ESP_INTR_FLAG_SHARED); //enable shared interrupts
  io_setup();
  mbus_setup();
  network_setup();
}

// entry point
void app_main(void) {
    printf("[MAIN] Booting...\n");
    app_setup();

    for( ;; ) { // main loop, as we use tasks, timers and interrupts, this should stay clean.
      vTaskDelay(1000 / portTICK_RATE_MS);
    }
}