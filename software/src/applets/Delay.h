#pragma once

#include "HemisphereAudioApplet.h"
#include <Audio.h>
#include <AudioDelayExt.h>

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
    float d = delaySecs + InF(0) * MAX_DELAY_SECS * 0.25f;
    CONSTRAIN(d, 0.0f, MAX_DELAY_SECS);
    for (int tap = 0; tap < taps; tap++) {
      delay.delay(tap, d * (tap + 1) / taps);
    }
    for (int i = 0; i < 4; i++) {
      taps_mixer1.gain(i, i < taps ? 1.0f : 0.0f);
      int j = i + 4;
      taps_mixer2.gain(i, j < taps ? 1.0f : 0.0f);
    }

    float w = wet + InF(1);
    CONSTRAIN(w, 0.0f, 1.0f);
    feedback_mixer.gain(FB_WET_CH_1, feedback / taps);
    feedback_mixer.gain(FB_WET_CH_2, feedback / taps);
    wet_dry_mixer.gain(WD_WET_CH_1, w);
    wet_dry_mixer.gain(WD_WET_CH_2, w);
    wet_dry_mixer.gain(WD_DRY_CH, 1.0f - w);
  }
  void View() {
    gfxPrint(0, 15, delaySecs * 1000.0f, 0);
    gfxPrint("ms");
    if (cursor == TIME)
      gfxCursor(0, 23, 6 * 7);
    gfxPrint(0, 25, "FB: ");
    gfxPrint(100 * feedback, 0);
    gfxPrint("%");
    if (cursor == FEEDBACK)
      gfxCursor(0, 32, 6 * 8);
    gfxPrint(0, 35, "Wet: ");
    gfxPrint(100 * wet, 0);
    gfxPrint("%");
    if (cursor == WET)
      gfxCursor(0, 42, 6 * 9);
    gfxPrint(0, 45, "Taps: ");
    gfxPrint(taps);
    if (cursor == TAPS)
      gfxCursor(0, 52, 6 * 7);
  }
  void OnButtonPress() { CursorToggle(); }
  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }

    switch (cursor) {
    case TIME:
      delaySecs += direction * 128 / AUDIO_SAMPLE_RATE;
      CONSTRAIN(delaySecs, 0.0f, MAX_DELAY_SECS);
      break;
    case FEEDBACK:
      feedback += 0.01f * direction;
      CONSTRAIN(feedback, 0.0f, 1.0f);
      break;
    case WET:
      wet += 0.01f * direction;
      CONSTRAIN(wet, 0.0f, 1.0f);
      break;
    case TAPS:
      taps += direction;
      CONSTRAIN(taps, 1, 8);
    case MAX_CURSOR:
      break;
    }
  }
  uint64_t OnDataRequest() { return 0; }
  void OnDataReceive(uint64_t data) {}

protected:
  void SetHelp() {}

private:
  enum Cursor {
    TIME,
    FEEDBACK,
    WET,
    TAPS,
    MAX_CURSOR,
  };

  int cursor = TIME;

  float delaySecs = 0.1f;
  float wet = 1.0f;
  float feedback = 0.0f;
  int taps = 1;

  const uint8_t FB_DRY_CH = 0;
  const uint8_t FB_WET_CH_1 = 1;
  const uint8_t FB_WET_CH_2 = 2;
  const uint8_t WD_DRY_CH = 0;
  const uint8_t WD_WET_CH_1 = 1;
  const uint8_t WD_WET_CH_2 = 2;

  // Uses 1MB of psram and gives just under 12 secs of delay time.
  static const size_t DELAY_LENGTH = 1024 * 512; // 1024 * 512;
  static constexpr float MAX_DELAY_SECS = DELAY_LENGTH / AUDIO_SAMPLE_RATE;

  AudioDelayExt<DELAY_LENGTH, 8> delay;
  AudioMixer4 feedback_mixer;
  AudioConnection input_conn{input_stream, 0, feedback_mixer, FB_DRY_CH};
  AudioConnection delay_in{feedback_mixer, 0, delay, 0};
  AudioConnection taps_conns[8];
  AudioMixer4 taps_mixer1;
  AudioMixer4 taps_mixer2;

  AudioConnection feedback_out1{taps_mixer1, 0, feedback_mixer, FB_WET_CH_1};
  AudioConnection feedback_out2{taps_mixer2, 0, feedback_mixer, FB_WET_CH_2};

  AudioMixer4 wet_dry_mixer;
  AudioConnection wet_conn1{taps_mixer1, 0, wet_dry_mixer, WD_WET_CH_1};
  AudioConnection wet_conn2{taps_mixer2, 0, wet_dry_mixer, WD_WET_CH_2};
  AudioConnection dry_conn{input_stream, 0, wet_dry_mixer, WD_DRY_CH};
  AudioConnection output_conn{wet_dry_mixer, output_stream};
};
