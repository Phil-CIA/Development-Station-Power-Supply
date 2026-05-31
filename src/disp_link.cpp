#include "disp_link.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

namespace disp_link {

namespace {

constexpr uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t s_seq    = 0;
bool    s_begun  = false;

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

void onSendStatus(const wifi_tx_info_t* /*info*/, esp_now_send_status_t status) {
  // Quiet on success to avoid spam; only log failures.
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("disp_link: esp_now send FAIL");
  }
}

}  // namespace

void begin() {
  if (s_begun) return;

  // UART1 on the C6's UART1 peripheral, RX=GPIO0, TX=GPIO7. Wired to the
  // CrowPanel UART1-OUT HY2.0-4P connector through the CH486F mux (K1=0,1).
  Serial1.begin(kUartBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);
  Serial.printf("disp_link: UART1 up @%lu baud RX=GPIO%d TX=GPIO%d\n",
                static_cast<unsigned long>(kUartBaud), kUartRxPin, kUartTxPin);

  // STA mode, no association — required for ESP-NOW.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(50);

  esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE);

  const esp_err_t init_rc = esp_now_init();
  if (init_rc != ESP_OK) {
    Serial.printf("disp_link: esp_now_init FAIL rc=%d\n", init_rc);
    return;
  }
  esp_now_register_send_cb(onSendStatus);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcastMac, 6);
  peer.channel = kEspNowChannel;
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;
  const esp_err_t add_rc = esp_now_add_peer(&peer);
  if (add_rc != ESP_OK && add_rc != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("disp_link: esp_now_add_peer FAIL rc=%d\n", add_rc);
    return;
  }

  s_begun = true;

  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  Serial.printf("disp_link: ESP-NOW up; STA MAC=%02X:%02X:%02X:%02X:%02X:%02X channel=%u broadcast peer ok\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], kEspNowChannel);
}

bool publishTelemetry(uint16_t v12_mV, int16_t i12_mA) {
  if (!s_begun) return false;

  uint8_t frame[kFrameSizeTlm];
  frame[0] = kFrameSof1;
  frame[1] = kFrameSof2;
  frame[2] = kFrameLenTlm;
  frame[3] = kFrameTagTlm;
  frame[4] = s_seq++;
  frame[5] = static_cast<uint8_t>(v12_mV & 0xFF);
  frame[6] = static_cast<uint8_t>((v12_mV >> 8) & 0xFF);
  const uint16_t i_u = static_cast<uint16_t>(i12_mA);
  frame[7] = static_cast<uint8_t>(i_u & 0xFF);
  frame[8] = static_cast<uint8_t>((i_u >> 8) & 0xFF);
  frame[9] = crc8(&frame[2], 7);

  // Primary wired path: push the frame down UART1 (via CrowPanel CH486F mux
  // to S3 IO19/IO20; see repo memory entry "UART HAT↔CrowPanel — SOLVED").
  Serial1.write(frame, kFrameSizeTlm);

  // Send via ESP-NOW — wireless backup, always on.
  const esp_err_t rc = esp_now_send(kBroadcastMac, frame, kFrameSizeTlm);
  if (rc != ESP_OK) {
    Serial.printf("disp_link: esp_now_send rc=%d\n", rc);
    return false;
  }
  return true;
}

}  // namespace disp_link
