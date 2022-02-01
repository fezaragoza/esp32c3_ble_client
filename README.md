| Supported Targets | ESP32 | ESP32-C3 |
| ----------------- | ----- | -------- |

ESP32C3 BLE CLIENT LIB
========================

BLE Client library for ESP32C3 using GAP and GATT ESP_APIs

To test this, you can run the [gatt_server_demo](../gatt_server), which creates services and starts advertising. `Gatt_client_demo` will start scanning and connect to the `gatt_server_demo` automatically.

This library will enable gatt server's notification function once the connection is established and then the devices start exchanging data.

<!-- Please check the [tutorial](tutorial/Gatt_Client_Example_Walkthrough.md) for more information about this example. -->
