This custom ESPHome component implements WIFI repeater/extender functionality
for ESP32s. Specifically, it runs a SoftAP (in addition to the existing wifi connection), and configures it to perform NAT. Only works with the `esp-idf` framework (not Arduino).

> **_Warning:_** very hacky.

Usage:
```
esphome:
  ...
  includes:
    - nat-ap.h

esp32:
  board: esp32dev
  framework:
    type: esp-idf
    version: 4.4.5
    sdkconfig_options:
      CONFIG_COMPILER_OPTIMIZATION_SIZE: y
      CONFIG_LWIP_IP_FORWARD: y
      CONFIG_LWIP_IPV4_NAPT: y

wifi:
  ...

custom_component:
  - id: nat
    lambda: return { new NatAp("SSID", "PASSWORD") };
```
