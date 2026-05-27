#pragma once

#include <Arduino.h>
#include <SDL_Arduino_INA3221.h>

// Hat-board logical rails. Names follow the KiCad net intent, not physical placement.
enum class HatRail : uint8_t {
  Rail5V = 0,
  Rail3V3,
  RailAdj,
  RailIncoming12V,
  Count
};

enum class HatRange : uint8_t {
  High = 0,
  Low,
  Single
};

struct InaTap {
  uint8_t address;
  uint8_t channel;    // INA3221 channels are 1..3.
  float shuntOhms;    // Physical shunt resistor in Ohms — used to convert shunt voltage to current.
};

struct RailMapping {
  const char* railName;
  InaTap highRange;
  InaTap lowRange;
  InaTap singleRange;
  bool hasDualRange;
  const char* regulatorFeedbackNet;
  const char* regulatorIsetNet;
  const char* regulatorControlNet;
};

struct RailMeasurement {
  float busVoltageV;
  float currentmA;
  HatRange usedRange;
};

struct RailCalibration {
  float voltageGain;
  float voltageOffset;
  float currentGain;
  float currentOffset;
};

class HatPowerTelemetry {
 public:
  explicit HatPowerTelemetry(SDL_Arduino_INA3221& driver) : ina_(driver) {
    setInaAddresses(0x40, 0x41, 0x42);
    setAutoRangeThresholds(250.0f, 900.0f);
    resetCalibration();
  }

  void setInaAddresses(uint8_t addr5V, uint8_t addr3V3, uint8_t addrAdj) {
    // U1 = 5V rails, U2 = 3.3V + incoming 12V at CH3, U3 = adjustable rails.
    // Shunt defaults from BOM: HIGH=0.020 Ohm, LOW=0.200 Ohm, Incoming12V=0.010 Ohm.
    // Confirm actual PCB values with a DMM then update via SHUNT command + CFGSAVE.
    map_[static_cast<size_t>(HatRail::Rail5V)] = {
      "5V rail", {addr5V, 1, 0.020f}, {addr5V, 2, 0.200f}, {addr5V, 1, 0.020f}, true,
      "Feedback 5V", "ISET_MPU_5V", "+5V_Control"
    };

    map_[static_cast<size_t>(HatRail::Rail3V3)] = {
      "3.3V rail", {addr3V3, 1, 0.020f}, {addr3V3, 2, 0.200f}, {addr3V3, 1, 0.020f}, true,
      "Feedback 3.3", "ISET_MPU_3V3", "(none)"
    };

    map_[static_cast<size_t>(HatRail::RailAdj)] = {
      "Adjustable rail", {addrAdj, 1, 0.020f}, {addrAdj, 2, 0.200f}, {addrAdj, 1, 0.020f}, true,
      "Feedback Adj Channel", "ISET_MPU_Channel_3", "3.3_Select"
    };

    map_[static_cast<size_t>(HatRail::RailIncoming12V)] = {
      "Incoming 12V", {addr3V3, 3, 0.010f}, {addr3V3, 3, 0.010f}, {addr3V3, 3, 0.010f}, false,
      "Incoming (+/-)", "(none)", "(none)"
    };
  }

  const RailMapping& mapping(HatRail rail) const {
    return map_[static_cast<size_t>(rail)];
  }

  void setAutoRangeThresholds(float switchToLowmA, float switchToHighmA) {
    lowThresholdmA_ = switchToLowmA;
    highThresholdmA_ = switchToHighmA;
  }

  float switchToLowmA() const { return lowThresholdmA_; }
  float switchToHighmA() const { return highThresholdmA_; }

  void setCalibration(HatRail rail, HatRange range, const RailCalibration& cal) {
    cal_[static_cast<size_t>(rail)][rangeIndex(range)] = cal;
  }

  RailCalibration calibration(HatRail rail, HatRange range) const {
    return cal_[static_cast<size_t>(rail)][rangeIndex(range)];
  }

  void resetCalibration() {
    const RailCalibration unity = {1.0f, 0.0f, 1.0f, 0.0f};
    for (size_t rail = 0; rail < static_cast<size_t>(HatRail::Count); rail++) {
      for (size_t idx = 0; idx < 3; idx++) {
        cal_[rail][idx] = unity;
      }
    }
  }

  void setShuntOhms(HatRail rail, HatRange range, float ohms) {
    tapRef(rail, range).shuntOhms = ohms;
  }

  float shuntOhms(HatRail rail, HatRange range) const {
    const RailMapping& m = map_[static_cast<size_t>(rail)];
    if (range == HatRange::Low)  return m.lowRange.shuntOhms;
    if (range == HatRange::High) return m.highRange.shuntOhms;
    return m.singleRange.shuntOhms;
  }

  RailMeasurement read(HatRail rail, HatRange preferredRange = HatRange::High) {
    const RailMapping& cfg = mapping(rail);

    if (!cfg.hasDualRange) {
      return {readBusVoltageV(cfg.singleRange), readCurrentmA(cfg.singleRange), HatRange::Single};
    }

    const InaTap tap = (preferredRange == HatRange::Low) ? cfg.lowRange : cfg.highRange;
    return {readBusVoltageV(tap), readCurrentmA(tap), preferredRange};
  }

  RailMeasurement readCalibrated(HatRail rail, HatRange preferredRange = HatRange::High) {
    RailMeasurement raw = read(rail, preferredRange);
    applyCalibration(rail, raw);
    return raw;
  }

  RailMeasurement readAuto(HatRail rail) {
    const RailMapping& cfg = mapping(rail);

    if (!cfg.hasDualRange) {
      return read(rail, HatRange::Single);
    }

    RailMeasurement hi = read(rail, HatRange::High);
    RailMeasurement lo = read(rail, HatRange::Low);

    const size_t idx = static_cast<size_t>(rail);
    const float absHigh = fabsf(hi.currentmA);
    const float absLow = fabsf(lo.currentmA);

    HatRange next = activeRange_[idx];
    if (next == HatRange::Low) {
      if (absLow >= highThresholdmA_) {
        next = HatRange::High;
      }
    } else {
      if (absHigh <= lowThresholdmA_) {
        next = HatRange::Low;
      }
    }

    activeRange_[idx] = next;
    return (next == HatRange::Low) ? lo : hi;
  }

  RailMeasurement readAutoCalibrated(HatRail rail) {
    RailMeasurement raw = readAuto(rail);
    applyCalibration(rail, raw);
    return raw;
  }

  void resetActiveRangesToHigh() {
    activeRange_[static_cast<size_t>(HatRail::Rail5V)] = HatRange::High;
    activeRange_[static_cast<size_t>(HatRail::Rail3V3)] = HatRange::High;
    activeRange_[static_cast<size_t>(HatRail::RailAdj)] = HatRange::High;
    activeRange_[static_cast<size_t>(HatRail::RailIncoming12V)] = HatRange::Single;
  }

  void printTopology(Stream& out) const {
    out.println("[HWMAP] Hat power telemetry mapping");
    out.println("[HWMAP] Rail, INA addr, CH high, CH low/single, feedback, iset, control");

    for (size_t i = 0; i < static_cast<size_t>(HatRail::Count); i++) {
      const RailMapping& cfg = map_[i];
      out.printf("[HWMAP] %s, 0x%02X, CH%u, ", cfg.railName, cfg.highRange.address, cfg.highRange.channel);
      if (cfg.hasDualRange) {
        out.printf("CH%u, ", cfg.lowRange.channel);
      } else {
        out.printf("single(CH%u), ", cfg.singleRange.channel);
      }
      out.printf("%s, %s, %s\n", cfg.regulatorFeedbackNet, cfg.regulatorIsetNet, cfg.regulatorControlNet);
    }

    out.println("[HWMAP] Connector map: J1->J4 and J2->J6 net-for-net");
  }

  void printCalibration(Stream& out) const {
    out.println("[CAL] rail,range,vgain,voffset,igain,ioffset");
    for (size_t rail = 0; rail < static_cast<size_t>(HatRail::Count); rail++) {
      const RailMapping& cfg = map_[rail];
      for (size_t ridx = 0; ridx < 3; ridx++) {
        const HatRange range = static_cast<HatRange>(ridx);
        if (!cfg.hasDualRange && range != HatRange::Single) {
          continue;
        }
        if (cfg.hasDualRange && range == HatRange::Single) {
          continue;
        }
        const RailCalibration& c = cal_[rail][ridx];
        out.printf("[CAL] %s,%s,%.6f,%.6f,%.6f,%.6f\n",
                   cfg.railName,
                   rangeName(range),
                   c.voltageGain,
                   c.voltageOffset,
                   c.currentGain,
                   c.currentOffset);
      }
    }
  }

  void printShuntConfig(Stream& out) const {
    out.println("[SHUNT] rail,range,shuntOhms");
    for (size_t i = 0; i < static_cast<size_t>(HatRail::Count); i++) {
      const RailMapping& cfg = map_[i];
      if (cfg.hasDualRange) {
        out.printf("[SHUNT] %s,HIGH,%.6f\n", cfg.railName, cfg.highRange.shuntOhms);
        out.printf("[SHUNT] %s,LOW,%.6f\n",  cfg.railName, cfg.lowRange.shuntOhms);
      } else {
        out.printf("[SHUNT] %s,SINGLE,%.6f\n", cfg.railName, cfg.singleRange.shuntOhms);
      }
    }
  }

  static const char* rangeName(HatRange range) {
    switch (range) {
      case HatRange::High: return "HIGH";
      case HatRange::Low: return "LOW";
      default: return "SINGLE";
    }
  }

 private:
  static size_t rangeIndex(HatRange range) {
    return static_cast<size_t>(range);
  }

  InaTap& tapRef(HatRail rail, HatRange range) {
    RailMapping& m = map_[static_cast<size_t>(rail)];
    if (range == HatRange::Low)  return m.lowRange;
    if (range == HatRange::High) return m.highRange;
    return m.singleRange;
  }

  void applyCalibration(HatRail rail, RailMeasurement& reading) const {
    const RailCalibration& c = cal_[static_cast<size_t>(rail)][rangeIndex(reading.usedRange)];
    reading.busVoltageV = (reading.busVoltageV * c.voltageGain) + c.voltageOffset;
    reading.currentmA = (reading.currentmA * c.currentGain) + c.currentOffset;
  }

  float readBusVoltageV(const InaTap& tap) {
    selectAddress(tap.address);
    return ina_.getBusVoltage_V(static_cast<int>(tap.channel));
  }

  float readCurrentmA(const InaTap& tap) {
    selectAddress(tap.address);
    // Compute current from shunt voltage using the tap-specific shunt resistance.
    // This keeps each channel's shunt value independent without mutating shared library state.
    const float shuntmV = ina_.getShuntVoltage_mV(static_cast<int>(tap.channel));
    return shuntmV / tap.shuntOhms;
  }

  void selectAddress(uint8_t address) {
    if (activeAddress_ == address) {
      return;
    }
    ina_.INA3221_i2caddr = address;
    ina_.INA3221SetConfig();
    activeAddress_ = address;
  }

  SDL_Arduino_INA3221& ina_;
  RailMapping map_[static_cast<size_t>(HatRail::Count)] = {};
  RailCalibration cal_[static_cast<size_t>(HatRail::Count)][3] = {};
  HatRange activeRange_[static_cast<size_t>(HatRail::Count)] = {
    HatRange::High, HatRange::High, HatRange::High, HatRange::Single
  };
  float lowThresholdmA_ = 250.0f;
  float highThresholdmA_ = 900.0f;
  uint8_t activeAddress_ = 0xFF;
};
