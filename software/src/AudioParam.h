#pragma once

#include <Audio.h>

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);

template <typename T> class AudioParam {
public:
  AudioParam() : AudioParam(T()) {}

  AudioParam(T v, float lpf_coeff = 1.0f)
      : value(v), prev(v), lpf(v), inc(0), lpf_coeff(lpf_coeff) {}

  AudioParam &operator=(const T &newValue) {
    value = newValue;
    inc = (value - prev) / AUDIO_BLOCK_SAMPLES;
    return *this;
  }

  T ReadNext() {
    prev += inc;
    // check if the sign on inc and value-prev are different to see if we've
    // gone too far
    if (inc * (value - prev) <= 0.0f) {
      inc = 0.0f;
      prev = value;
    }
    ONE_POLE(lpf, prev, lpf_coeff);
    return lpf;
  }

  T Read() {
    return lpf;
  }

  void Reset() {
    inc = 0;
    lpf = prev = value;
  }

private:
  T value;
  T prev;
  T lpf;
  T inc;
  float lpf_coeff;
};

