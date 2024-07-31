#pragma once

#include "AudioDelayExt.h"
#include "HSicons.h"
#include "HemisphereAudioApplet.h"
#include "dsputils.h"
#include <Audio.h>

class Delay : public HemisphereAudioApplet<Mono> {
public:
  Delay() {
    for (int i = 0; i < 4; i++) {
      taps_conns[i].connect(delay, i, taps_mixer1, i);
      int j = i + 4;
      taps_conns[j].connect(delay, j, taps_mixer2, i);
    }
  }
  const char *applet_name() { return "Delay"; }
  void Start() {}

  void Controller() {
    clock_count++;
    if (Clock(0)) {
      clock_base_secs = clock_count / 16666.0f;
      clock_count = 0;
    }
    float d = DelaySecs(delay_exp + In(0));
    float f = 0.01f * feedback / taps;
    for (int tap = 0; tap < taps; tap++) {
      CONSTRAIN(d, 0.0f, MAX_DELAY_SECS);
      delay.delay(tap, d * (tap + 1) / taps);
      delay.feedback(tap, f);
    }
    for (int tap = taps; tap < 8; tap++) {
      delay.feedback(tap, 0);
    }
    for (int i = 0; i < 4; i++) {
      taps_mixer1.gain(i, i < taps ? 1.0f : 0.0f);
      int j = i + 4;
      taps_mixer2.gain(i, j < taps ? 1.0f : 0.0f);
    }

    float w = 0.01f * wet + InF(1);
    CONSTRAIN(w, 0.0f, 1.0f);
    wet_dry_mixer.gain(WD_WET_CH_1, w);
    wet_dry_mixer.gain(WD_WET_CH_2, w);
    wet_dry_mixer.gain(WD_DRY_CH, 1.0f - w);
  }
  void View() {
    // gfxPrint(0, 15, delaySecs * 1000.0f, 0);
    switch (time_rep) {
    case SECS:
      gfxPrint(0, 15, DelaySecs(delay_exp) * 1000.0f, 0);
      gfxPrint(6 * 6, 15, "ms");
      break;
    case HZ:
      gfxPrint(0, 15, 1.0f / DelaySecs(delay_exp), 2);
      gfxPrint(6 * 6, 15, "Hz");
      break;
    case CLOCK:
      float r = DelayRatio(delay_exp);
      if (r < 1.0f) {
        gfxPrint(0, 15, "/ ");
        gfxPrint(roundf(1.0f / r), 0);
      } else {
        gfxPrint(0, 15, "X ");
        gfxPrint(roundf(r), 0);
      }
      gfxIcon(6 * 6, 15, CLOCK_ICON);
      break;
    }
    if (cursor == TIME)
      gfxCursor(0, 23, 6 * 6);
    if (cursor == TIME_REP)
      gfxCursor(6 * 6, 23, 2 * 6);

    gfxPrint(0, 25, "FB: ");
    gfxPrint(feedback);
    gfxPrint("%");
    if (cursor == FEEDBACK)
      gfxCursor(0, 32, 6 * 8);
    gfxPrint(0, 35, "Wet: ");
    gfxPrint(wet);
    gfxPrint("%");
    if (cursor == WET)
      gfxCursor(0, 42, 6 * 9);
    gfxPrint(0, 45, "Taps: ");
    gfxPrint(taps);
    if (cursor == TAPS)
      gfxCursor(0, 52, 6 * 7);
    gfxPos(0, 55);
    graphics.printf("%3d%%/%3d%%", static_cast<int>(delay.processorUsage()),
                    static_cast<int>(delay.processorUsageMax()));
  }
  void OnButtonPress() { CursorToggle(); }
  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, CURSOR_LENGTH - 1);
      return;
    }

    float cur_delay;

    switch (cursor) {
    case TIME:
      if (time_rep == CLOCK) {
        cur_delay = DelayRatio(delay_exp);
        if (cur_delay < 1.0f || (roundf(cur_delay) == 1.0f && direction > 0)) {
          delay_exp = -RatioToPitch(1.0f / cur_delay + direction);
        } else {
          delay_exp = RatioToPitch(cur_delay - direction);
        }
      } else {
        cur_delay = DelaySecs(delay_exp);
        while (DelaySecs(delay_exp) == cur_delay)
          delay_exp += direction;
      }
      break;
    case TIME_REP:
      time_rep += direction;
      CONSTRAIN(time_rep, 0, TIME_REP_LENGTH - 1);
      break;
    case FEEDBACK:
      feedback += direction;
      CONSTRAIN(feedback, 0, 100);
      break;
    case WET:
      wet += direction;
      CONSTRAIN(wet, 0, 100);
      break;
    case TAPS:
      taps += direction;
      CONSTRAIN(taps, 1, 8);
    case CURSOR_LENGTH:
      break;
    }
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, delay_loc, delay_exp);
    Pack(data, time_rep_loc, time_rep);
    Pack(data, wet_loc, wet);
    Pack(data, fb_loc, feedback);
    Pack(data, taps_loc, taps - 1);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    if (data != 0) {
      delay_exp = Unpack(data, delay_loc);
      time_rep = Unpack(data, time_rep_loc);
      wet = Unpack(data, wet_loc);
      feedback = Unpack(data, fb_loc);
      taps = Unpack(data, taps_loc) + 1;
    }
  }

  float DelaySecs(int16_t exp) {
    // we need duration, not frequency: negating pitch gets that faster than
    // `1.0f /`
    float s;
    switch (time_rep) {
    case HZ:
      // This should keep the number of ms and hz the same, which should be
      // asthetically pleasing at least.
      s = (PitchToRatio(-exp)) / 500.0f;
      break;
    case CLOCK:
      s = clock_base_secs / (DelayRatio(exp));
      break;
    default:
    case SECS:
      s = 0.5f * (PitchToRatio(exp));
      break;
    }
    CONSTRAIN(s, 0.0f, MAX_DELAY_SECS);
    return s;
  }

  float DelayRatio(int16_t exp) {
    if (exp < 0) {
      return 1.0f / roundf(PitchToRatio(-exp));
    } else {
      return roundf(PitchToRatio(exp));
    }
  }

protected:
  void SetHelp() {}

private:
  enum Cursor {
    TIME,
    TIME_REP,
    FEEDBACK,
    WET,
    TAPS,
    CURSOR_LENGTH,
  };

  enum TimeRep {
    SECS,
    CLOCK,
    HZ,
    TIME_REP_LENGTH,
  };

  int cursor = TIME;

  int16_t delay_exp = 0;
  uint8_t time_rep = 0;
  // Only need 7 bits on these but the sign makes CONSTRAIN work
  int8_t wet = 50;
  int8_t feedback = 0;
  int8_t taps = 1;
  PackLocation delay_loc{0, 16};
  PackLocation time_rep_loc{16, 4};
  PackLocation wet_loc{32, 7};
  PackLocation fb_loc{39, 7};
  PackLocation taps_loc{46, 3};

  uint32_t clock_count = 0;
  float clock_base_secs = 0.0f;

  const uint8_t WD_DRY_CH = 0;
  const uint8_t WD_WET_CH_1 = 1;
  const uint8_t WD_WET_CH_2 = 2;

  // Uses 1MB of psram and gives just under 12 secs of delay time.
  static const size_t DELAY_LENGTH = 1024 * 512; // 1024 * 512;
  static constexpr float MAX_DELAY_SECS = DELAY_LENGTH / AUDIO_SAMPLE_RATE;

  AudioDelayExt<DELAY_LENGTH, 8> delay;
  AudioConnection input_conn{input_stream, delay};
  AudioConnection taps_conns[8];
  AudioMixer4 taps_mixer1;
  AudioMixer4 taps_mixer2;

  AudioMixer4 wet_dry_mixer;
  AudioConnection wet_conn1{taps_mixer1, 0, wet_dry_mixer, WD_WET_CH_1};
  AudioConnection wet_conn2{taps_mixer2, 0, wet_dry_mixer, WD_WET_CH_2};
  AudioConnection dry_conn{input_stream, 0, wet_dry_mixer, WD_DRY_CH};
  AudioConnection output_conn{wet_dry_mixer, output_stream};
};
