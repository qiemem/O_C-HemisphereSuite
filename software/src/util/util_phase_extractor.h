#pragma once

#include "util/util_pattern_predictor.h"

struct Ratio {
  int16_t num;
  uint16_t den;
};

template <size_t history_size = 32, uint8_t max_candidate_period = 8>
class PhaseExtractor {
public:
  PhaseExtractor() {}

  void Init() {
    predictor.Init();
    phase = 0;
    ticks = 0;
    // clocks_received = 0;
  }

  // -2 => /3, -1 => /2, 0 => 1, 1 => x2, 2 => x3, etc
  uint32_t Advance(bool clock, int simple_ratio) {
    return Advance(clock, {static_cast<int16_t>(max(simple_ratio + 1, 1)),
                           static_cast<uint16_t>(max(-simple_ratio + 1, 1))});
  }

  uint32_t Advance(bool clock, Ratio r) {
    if (clock) {
      next_clock_tick = predictor.Predict(ticks);
      ticks = 0;
      clocks_received++;
      if (clocks_received >= ratio.den) {
        // Syncing on the downbeat of previous divisor ensures we won't get any
        // discontinuities in the phase
        clocks_received = 0;
        ratio = r;
        phase = 0;
      }
      phase_inc = 0xffffffff / (ratio.den * next_clock_tick) * ratio.num;
    } else {
      ticks++;
    }
    phase += phase_inc;
    if (ticks > next_clock_tick) {
      // We've gone too far; check if we've rolled over as well and if so, pause
      // to wait for the next clock
      // Note: I haven't actually tested this with negative ratios.
      if (phase_inc > 0 && phase <= static_cast<uint32_t>(phase_inc))
        phase = 0xffffffff;
      else if (phase_inc < 0 && phase >= static_cast<uint32_t>(phase_inc))
        phase = 0;
    }
    return phase;
  }

private:
  stmlib::PatternPredictor<history_size, max_candidate_period> predictor;
  uint32_t clocks_received;
  Ratio ratio;
  uint32_t next_clock_tick;
  uint32_t ticks;
  uint32_t phase;
  int32_t phase_inc;
};
