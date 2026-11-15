#include "network_status.h"

#include "esphome/components/wifi/wifi_component.h"

bool NetworkStatus::is_connected() {
  return esphome::wifi::global_wifi_component != nullptr &&
         esphome::wifi::global_wifi_component->is_connected();
}

int8_t NetworkStatus::rssi() {
  if (!NetworkStatus::is_connected()) return esphome::wifi::WIFI_RSSI_DISCONNECTED;
  return esphome::wifi::global_wifi_component->wifi_rssi();
}

int32_t NetworkStatus::channel() {
  if (!NetworkStatus::is_connected()) return 0;
  return esphome::wifi::global_wifi_component->get_wifi_channel();
}
