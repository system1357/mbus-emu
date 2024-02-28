#ifndef __IO_H
#define __IO_H

#include "include/config.h"

#define GPIO_INPUT_CDC CONFIG_GPIO_INPUT_CDC_OUT
#define GPIO_INPUT_BT CONFIG_GPIO_INPUT_BT_OUT
#define GPIO_INPUT_INTCD CONFIG_GPIO_INPUT_INTCD_OUT
#define GPIO_INPUT_AUX CONFIG_GPIO_INPUT_AUX_OUT

#define GPIO_INTCD_STATUS_IN CONFIG_GPIO_INTCD_STATUS_IN
#define GPIO_INTCD_STATUS_OUT CONFIG_GPIO_INTCD_STATUS_OUT

#define GPIO_BT_INPUT_AUDIO CONFIG_GPIO_BT_INPUT_AUDIO
#define GPIO_BT_INPUT_CALL CONFIG_GPIO_BT_INPUT_CALL


typedef enum {
    inCDC,
    inBT,
    inIntCD,
    inAUX
} input_t;

typedef enum {
    btPlay,
    btPause,
    btNext,
    btPrev,
    btCallPickup,
    btCallHangup
} btButton_t;

void io_setup();
void io_debug(bool newState);
void io_switchInput(int input);

void IRAM_ATTR gpio_isr_bt_input_audio(void* arg);
void IRAM_ATTR gpio_isr_bt_input_call(void* arg);
void IRAM_ATTR gpio_isr_intcd_status(void* arg);
void io_bt_sendButton(int button);

#endif
