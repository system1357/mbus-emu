/*
 * Written by William Peters in 2021
 */

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

// App includes
#include "include/io.h"
#include "include/mbusEmu.h"

bool ioDebug=true;

void io_setup() {
    printf("[IO] Configuring GPIO...\t\t");
    gpio_set_direction(GPIO_INPUT_CDC, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_INPUT_BT, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_INPUT_INTCD, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_INPUT_AUX, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_INPUT_CDC,0);
    gpio_set_level(GPIO_INPUT_BT,0);
    gpio_set_level(GPIO_INPUT_INTCD,0);
    gpio_set_level(GPIO_INPUT_AUX,0);

    gpio_set_direction(GPIO_INTCD_STATUS_IN, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_BT_INPUT_AUDIO, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_BT_INPUT_CALL, GPIO_MODE_INPUT);

    gpio_set_intr_type(GPIO_INTCD_STATUS_IN, GPIO_INTR_ANYEDGE );
    gpio_set_intr_type(GPIO_BT_INPUT_AUDIO, GPIO_INTR_ANYEDGE );
    gpio_set_intr_type(GPIO_BT_INPUT_CALL, GPIO_INTR_ANYEDGE );

    gpio_isr_handler_add(GPIO_INTCD_STATUS_IN, gpio_isr_intcd_status, (void*) GPIO_INTCD_STATUS_IN);
    gpio_isr_handler_add(GPIO_BT_INPUT_AUDIO, gpio_isr_bt_input_audio, (void*) GPIO_BT_INPUT_AUDIO);
    gpio_isr_handler_add(GPIO_BT_INPUT_CALL, gpio_isr_bt_input_call, (void*) GPIO_BT_INPUT_CALL);
  
    gpio_intr_enable(GPIO_BT_INPUT_AUDIO);
    gpio_intr_enable(GPIO_BT_INPUT_CALL);
    printf("[ DONE ]\n");
}

void io_debug(bool newState) {
    ioDebug=true;
}

void io_switchInput(int input) {
    if(input==inCDC) {
        if(ioDebug) printf("Activating input CDC\n");
            gpio_set_level(GPIO_INPUT_CDC,1);
            gpio_set_level(GPIO_INPUT_BT,0);
            gpio_set_level(GPIO_INPUT_INTCD,0);
            gpio_set_level(GPIO_INPUT_AUX,0);
    } else if(input==inBT) {
        if(ioDebug) printf("Activating input BT\n");
            gpio_set_level(GPIO_INPUT_CDC,0);
            gpio_set_level(GPIO_INPUT_BT,1);
            gpio_set_level(GPIO_INPUT_INTCD,0);
            gpio_set_level(GPIO_INPUT_AUX,0);
    } else if(input==inIntCD) {
        if(ioDebug) printf("Activating input INT CD\n");
            gpio_set_level(GPIO_INPUT_CDC,0);
            gpio_set_level(GPIO_INPUT_BT,0);
            gpio_set_level(GPIO_INPUT_INTCD,1);
            gpio_set_level(GPIO_INPUT_AUX,0);
    } else if(input==inAUX) {
        if(ioDebug) printf("Activating input AUX\n");
            gpio_set_level(GPIO_INPUT_CDC,0);
            gpio_set_level(GPIO_INPUT_BT,0);
            gpio_set_level(GPIO_INPUT_INTCD,0);
            gpio_set_level(GPIO_INPUT_AUX,1);
    } else {
        if(ioDebug) printf("Unknown input:%i!\n", input);
    }
}


void io_bt_sendButton(int button) {
    if(button==btPlay) {
        printf("Sending Play\n");
    } else if(button==btPause) {
        printf("Sending PAuse\n");
    } else if(button==btNext) {
        printf("Sending Next\n");
    } else if(button==btPrev) {
        printf("Sending Prev\n");
    } else if(button==btCallPickup) {
        printf("Sending CallPickup\n");
    } else if(button==btCallHangup) {
        printf("Sending CallHangup\n");
    } else {
        printf("Invalid btbutton:%i\n", button);
    }
}

void io_intcd_pause() {

}


void gpio_isr_bt_input_audio(void* arg) { //todo handle bt audio
    emuBTMode(gpio_get_level(GPIO_BT_INPUT_AUDIO));
}

void gpio_isr_bt_input_call(void* arg) { //todo handle bt call
    emuPhoneMode(gpio_get_level(GPIO_BT_INPUT_CALL));
}

void gpio_isr_intcd_status(void* arg) { //todo handle bt call
    //crash, perhaps two interrupts firing and both reading level. mutexes required?
    /* Oranje 10V cdplayer -> Radio cd playing
    * wit 10v radio -> cdplayer pause cd
    */
    //emuIntCDMode(gpio_get_level(GPIO_INTCD_STATUS_IN));
}