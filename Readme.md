# Alpine M-Bus CD Changer Emulator for ESP32

Source: https://esp32.com/viewtopic.php?t=21770#p82663

This repo is a cleanup of wllm3000's code from above link. 
The websockets and cd changer passthrough portion of the code was removed for my own usecase.
Bluetooth A2DP sink(I2S pin definition in Kconfig) & BLE is functional, but lack useful code for remote debugging or OTA functionality.

ToDo:
1. Implement Bluetooth OTA
2. Test on real head units
3. Head unit button triggering for other functions(play, pause, bt pairing...etc)

## Original Readme

This is my attempt to interface with a Honda branded Panasonic radio (1108) with an Alpine CHA-S614.

Warning, this code is still a complete mess with a lot of test and debugging functions spread all over the place. in short, this is more a Proof-Of-Concept than 

It's designed to run on a ESP32 Wrover.

What does it do: 
* MBUS with two interfaces (seperate CDC and Radio bus).
* Simple websockets debugging interface (libesphttpd)
* Passthrough communication between CDC and Radio but some basic emu is available. To enable CDC Emu mode press repeat on the HU 6 times.
* Print a lot of debug messages ;)


Compiling with vscode/esp-idf:
* Windows Users need to install nodejs and mingw64 with complete gcc (this is needed for tools from libespfs)
* Linux Users need nodejs


Credits/Sources and inspiration from (not in alphabetical order):
* Harald W. Leschner (https://github.com/picohari/atmega128_alpine-mbus-emulator)
* Jörg Hohensohn (http://www.hohensohn.info/mbus/)
* Marek Dębowski (https://web.archive.org/web/20060209073135/http://www.cdchangeren.prv.pl/)
* Oliver Mueller (https://github.com/Olstyle/MBus)
* (https://github.com/Spritetm/esphttpd-freertos)
* Jeroen Domburg/Chris Morgan: https://github.com/chmorgan/libesphttpd/ https://github.com/chmorgan/esphttpd-freertos
* Jeff Kent: https://github.com/jkent/libespfs



Disclamer:
I'm not responsible for any damage to your equipment, car, house. This code is far from complete or stable in anyway, the code is in pre-alpha stage, more like an experiment. It doesn't have any licsenses yet, I still need to figure this out how to make this compatible with the other licsenses. 