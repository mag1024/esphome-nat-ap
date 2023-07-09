#include "esphome.h"

#include "esp_event.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "dhcpserver/dhcpserver.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/lwip_napt.h"

static const char* TAG = "NatAp";

class NatAp : public Component {
 public:
  NatAp(const char* ssid, const char* password)
    : ssid_(ssid), password_(password) {}

  void setup() override;
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

  void loop() override {}

 private:
  const char* ssid_;
  const char* password_;
};

void wifi_event_handler(void*, esp_event_base_t, int32_t event_id, void* event_data) {
  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    auto* e = (wifi_event_ap_staconnected_t*)event_data;
    ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(e->mac), e->aid);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    auto* e = (wifi_event_ap_stadisconnected_t*)event_data;
    ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(e->mac), e->aid);
  }
}

void ap_config(esp_netif_t* ap_netif) {
  esp_err_t err;
  esp_netif_dhcps_stop(ap_netif);

  esp_netif_ip_info_t info;
  info.ip.addr = static_cast<uint32_t>(network::IPAddress(192, 168, 4, 1));
  info.gw.addr = info.ip.addr;
  info.netmask.addr = static_cast<uint32_t>(network::IPAddress(255, 255, 255, 0));
  err = esp_netif_set_ip_info(ap_netif, &info);
  if (err != ESP_OK) ESP_LOGE(TAG, "esp_netif_set_ip_info failed! %d", err);

  dhcps_lease_t lease;
  lease.enable = true;
  lease.start_ip.addr = info.ip.addr + htonl(99);
  lease.end_ip.addr = lease.start_ip.addr + htonl(100);
  err = esp_netif_dhcps_option(
      ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS,
      &lease, sizeof(lease));

  if (err != ESP_OK) ESP_LOGE(TAG, "esp_netif_dhcps_option failed! %d", err);

  dhcps_offer_t dhcps_dns_value = OFFER_DNS;
  dhcps_set_option_info(6, &dhcps_dns_value, sizeof(dhcps_dns_value));
  ip_addr_t dnsserver;
  dnsserver.addr = ipaddr_addr("8.8.8.8");
  dhcps_dns_setserver(&dnsserver);

  err = esp_netif_dhcps_start(ap_netif);
  if (err != ESP_OK) ESP_LOGE(TAG, "esp_netif_dhcps_start failed! %d", err);
}

void NatAp::setup() {
  ESP_LOGI(TAG, "Starting...");

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
	WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

  wifi_config_t conf = {};
  strcpy((char*)conf.ap.ssid, ssid_);
  strcpy((char*)conf.ap.password, password_);
  conf.ap.channel = 0;
  conf.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  conf.ap.max_connection = 3;
  conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &conf));

  esp_netif_t* ap_netif = esp_netif_next(nullptr);
  ap_config(ap_netif);

  esp_netif_ip_info_t if_info;
  ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &if_info));
  ESP_LOGI(TAG, "Started the AP: %s [" IPSTR "]", ssid_, IP2STR(&if_info.ip));

  while (!esp_netif_is_netif_up(ap_netif)) {
    ESP_LOGD(TAG, "Waiting for the AP to be up...");
    ets_delay_us(10000);
  }

  ESP_LOGI(TAG, "Enabling NAT");
  ip_napt_enable(if_info.ip.addr, 1);
}
