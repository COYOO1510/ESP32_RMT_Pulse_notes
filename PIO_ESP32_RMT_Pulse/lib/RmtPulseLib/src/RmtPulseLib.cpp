#include "RmtPulseLib.h"

#include <cmath>
#include <climits>
#include <memory>
#include <new>

#include <driver/gpio.h>
#include <driver/rmt.h>

namespace RmtPulseLib {
namespace {

constexpr uint8_t kRmtClockDivider = 80;       // APB 80MHz / 80 = 1MHz (1 tick = 1us)
constexpr uint16_t kMaxRmtDurationTicks = 32767;
constexpr float kDutyEpsilon = 0.0001f;

struct ChannelState {
  bool inUse = false;
  gpio_num_t pin = GPIO_NUM_MAX;
};

ChannelState g_channels[RMT_CHANNEL_MAX];
String g_lastError;

void setError(const String& message) {
  g_lastError = message;
}

rmt_channel_t findChannelByPin(gpio_num_t pin) {
  for (int i = 0; i < static_cast<int>(RMT_CHANNEL_MAX); ++i) {
    if (g_channels[i].inUse && g_channels[i].pin == pin) {
      return static_cast<rmt_channel_t>(i);
    }
  }
  return RMT_CHANNEL_MAX;
}

rmt_channel_t allocateChannel(gpio_num_t pin) {
  for (int i = 0; i < static_cast<int>(RMT_CHANNEL_MAX); ++i) {
    if (!g_channels[i].inUse) {
      rmt_channel_t channel = static_cast<rmt_channel_t>(i);
      rmt_config_t config = RMT_DEFAULT_CONFIG_TX(pin, channel);
      config.clk_div = kRmtClockDivider;
      config.tx_config.loop_en = false;
      config.tx_config.idle_output_en = true;
      config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

      esp_err_t err = rmt_config(&config);
      if (err != ESP_OK) {
        setError("rmt_config failed, err=" + String(err));
        return RMT_CHANNEL_MAX;
      }

      err = rmt_driver_install(channel, 0, 0);
      if (err != ESP_OK) {
        setError("rmt_driver_install failed, err=" + String(err));
        return RMT_CHANNEL_MAX;
      }

      g_channels[i].inUse = true;
      g_channels[i].pin = pin;
      return channel;
    }
  }

  setError("No free RMT TX channel");
  return RMT_CHANNEL_MAX;
}

rmt_channel_t acquireChannel(gpio_num_t pin) {
  rmt_channel_t existing = findChannelByPin(pin);
  if (existing != RMT_CHANNEL_MAX) {
    return existing;
  }
  return allocateChannel(pin);
}

}  // namespace

bool send(uint8_t pin, uint32_t pulseCount, uint32_t frequencyHz, float dutyPercent) {
  g_lastError = "";

  if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
    setError("Pin is not a valid output GPIO");
    return false;
  }
  if (pulseCount == 0) {
    setError("pulseCount must be > 0");
    return false;
  }
  if (frequencyHz == 0) {
    setError("frequencyHz must be > 0");
    return false;
  }
  if (dutyPercent < 0.0f || dutyPercent > 100.0f) {
    setError("dutyPercent must be in range 0.0 ~ 100.0");
    return false;
  }
  if (pulseCount > static_cast<uint32_t>(INT_MAX)) {
    setError("pulseCount is too large");
    return false;
  }

  const uint32_t periodUs = 1000000UL / frequencyHz;
  if (periodUs < 2) {
    setError("frequencyHz too high for duty-percent mode (period must be >= 2us)");
    return false;
  }

  const gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  rmt_channel_t channel = acquireChannel(gpio);
  if (channel == RMT_CHANNEL_MAX) {
    return false;
  }

  std::unique_ptr<rmt_item32_t[]> items(new (std::nothrow) rmt_item32_t[pulseCount]);
  if (!items) {
    setError("Not enough memory for pulse buffer");
    return false;
  }

  uint32_t highUs = 0;
  uint32_t lowUs = 0;
  uint16_t edgeD0 = 0;
  uint16_t edgeD1 = 0;
  const bool isZeroDuty = dutyPercent <= kDutyEpsilon;
  const bool isHundredDuty = dutyPercent >= (100.0f - kDutyEpsilon);

  if (!isZeroDuty && !isHundredDuty) {
    highUs = static_cast<uint32_t>(lroundf((static_cast<float>(periodUs) * dutyPercent) / 100.0f));
    if (highUs == 0) {
      highUs = 1;
    }
    if (highUs >= periodUs) {
      highUs = periodUs - 1;
    }
    lowUs = periodUs - highUs;

    if (highUs > kMaxRmtDurationTicks || lowUs > kMaxRmtDurationTicks) {
      setError("high/low time exceeds 32767us, lower frequency or adjust duty");
      return false;
    }
  } else {
    const uint32_t d0 = periodUs / 2;
    const uint32_t d1 = periodUs - d0;
    if (d0 == 0 || d1 == 0 || d0 > kMaxRmtDurationTicks || d1 > kMaxRmtDurationTicks) {
      setError("period cannot be represented at 0%/100% duty");
      return false;
    }
    edgeD0 = static_cast<uint16_t>(d0);
    edgeD1 = static_cast<uint16_t>(d1);
  }

  for (uint32_t i = 0; i < pulseCount; ++i) {
    if (isZeroDuty) {
      items[i].level0 = 0;
      items[i].duration0 = edgeD0;
      items[i].level1 = 0;
      items[i].duration1 = edgeD1;
    } else if (isHundredDuty) {
      items[i].level0 = 1;
      items[i].duration0 = edgeD0;
      items[i].level1 = 1;
      items[i].duration1 = edgeD1;
    } else {
      items[i].level0 = 1;
      items[i].duration0 = static_cast<uint16_t>(highUs);
      items[i].level1 = 0;
      items[i].duration1 = static_cast<uint16_t>(lowUs);
    }
  }

  const esp_err_t err = rmt_write_items(channel, items.get(), static_cast<int>(pulseCount), true);
  if (err != ESP_OK) {
    setError("rmt_write_items failed, err=" + String(err));
    return false;
  }

  return true;
}

void releasePin(uint8_t pin) {
  const gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  for (int i = 0; i < static_cast<int>(RMT_CHANNEL_MAX); ++i) {
    if (g_channels[i].inUse && g_channels[i].pin == gpio) {
      rmt_driver_uninstall(static_cast<rmt_channel_t>(i));
      g_channels[i].inUse = false;
      g_channels[i].pin = GPIO_NUM_MAX;
      return;
    }
  }
}

void releaseAll() {
  for (int i = 0; i < static_cast<int>(RMT_CHANNEL_MAX); ++i) {
    if (g_channels[i].inUse) {
      rmt_driver_uninstall(static_cast<rmt_channel_t>(i));
      g_channels[i].inUse = false;
      g_channels[i].pin = GPIO_NUM_MAX;
    }
  }
}

const char* lastError() {
  return g_lastError.c_str();
}

}  // namespace RmtPulseLib
