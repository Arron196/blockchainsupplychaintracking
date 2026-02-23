# stm32-firmware

Device firmware scope:

- Sensor sampling (temperature, humidity, CO2, soil pH, etc.).
- Data preprocessing and packet assembly.
- Hashing + ECDSA signing (ATECC608A optional).
- Communication via Wi-Fi (ESP8266) or LoRa (SX1278).
- Retry and fault reporting for unstable network links.

## Suggested source ownership

- `App/src/sensor_manager.c`: sensor reads and calibration.
- `App/src/telemetry_packet.c`: payload schema + serialization.
- `App/src/signer.c`: SHA256 + ECDSA signing wrapper.
- `App/src/comm_wifi.c`: Wi-Fi uplink implementation.
- `App/src/comm_lora.c`: LoRa uplink implementation.
- `App/src/app_main.c`: state machine and scheduler loop.
