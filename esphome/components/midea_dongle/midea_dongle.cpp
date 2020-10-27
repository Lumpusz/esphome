#include "midea_dongle.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace midea_dongle {

static const char *TAG = "midea_dongle";

void MideaDongle::loop() {
  while (this->available()) {
    const uint8_t rx = this->read();
    if (this->idx_ <= OFFSET_LENGTH) {
      if (this->idx_ == OFFSET_LENGTH) {
        if (rx <= OFFSET_BODY || rx >= sizeof(this->buf_)) {
          this->reset_();
          continue;
        }
        this->cnt_ = rx;
      } else if (rx != SYNC_BYTE) {
        continue;
      }
    }
    this->buf_[this->idx_++] = rx;
    if (--this->cnt_)
      continue;
    this->reset_();
    const BaseFrame frame(this->buf_);
    if (frame.get_type() == NETWORK_NOTIFY) {
      ESP_LOGD(TAG, "RX: notify frame");
      this->need_notify_ = false;
      continue;
    }
    if (!frame.is_valid()) {
      ESP_LOGW(TAG, "RX: frame check failed!");
      continue;
    }
    if (this->appliance_ != nullptr)
      this->appliance_->on_frame(frame);
  }
}

void MideaDongle::update() {
  const bool is_conn = WiFi.isConnected();
  uint8_t wifi_stretch = 0;
  if (this->wifi_sensor_ != nullptr && this->wifi_sensor_->has_state()) {
    const float dbm = this->wifi_sensor_->get_state();
    if (dbm > -62.5)
      wifi_stretch = 4;
    else if (dbm > -75.0)
      wifi_stretch = 3;
    else if (dbm > -87.5)
      wifi_stretch = 2;
    else if (dbm > -100.0)
      wifi_stretch = 1;
  } else if (is_conn) {
    wifi_stretch = 4;
  }
  if (this->notify_.is_connected() != is_conn) {
    this->notify_.set_connected(is_conn);
    this->need_notify_ = true;
  }
  if (this->notify_.get_signal_stretch() != wifi_stretch) {
    this->notify_.set_signal_stretch(wifi_stretch);
    this->need_notify_ = true;
  }
  if (!--this->notify_timer_)
    this->need_notify_ = true;
  if (this->need_notify_) {
    ESP_LOGD(TAG, "TX: notify WiFi STA %s, signal stretch %d", is_conn ? "connected" : "not connected", wifi_stretch);
    this->notify_timer_ = 600;
    this->notify_.finalize();
    this->write_frame(this->notify_);
    return;
  }
  if (this->appliance_ != nullptr)
    this->appliance_->on_update();
}

}  // namespace midea_dongle
}  // namespace esphome
