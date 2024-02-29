# Alpine M-Bus CD Changer Emulator for ESP32

Source: https://esp32.com/viewtopic.php?t=21770#p82663

This repo is a cleanup of wllm3000's code from above link. 
The original code is preserved in the branch original_code

The websockets and cd changer passthrough portion was removed for my own usecase.
Bluetooth A2DP sink(I2S pin definition in Kconfig) & BLE is functional, but lack useful code for remote debugging or OTA functionality.

ToDo:
1. Implement Bluetooth OTA
2. Test on real head units
3. Head unit button triggering for other functions(play, pause, bt pairing...etc)