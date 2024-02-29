esptool.py -p COM10 -b 460800 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_freq 80m --flash_size detect 0x10000 hello-world.bin 0x1000 bootloader.bin 0x8000 partition-table.bin 0xe000 ota_data_initial.bin

hello-world.bin
bootloader/bootloader.bin
partition_table/partition-table.bin
ota_data_initial.bin