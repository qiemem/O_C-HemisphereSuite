#include "HemisphereAudioApplet.h"
#include <Audio.h>

class Delay : public HemisphereAudioApplet<Mono> {
public:
  const char *applet_name() { return "Delay"; }
  void Start() { }

  void Controller() {
    float newDelay = InF(0) * 1000.0f;
    if (fabsf(newDelay - delayMs) > 2.0f) {
      delayMs = newDelay;
      delay.delay(0, delayMs);
    }
    feedback = InF(1);
    wet = InF(2);
    feedback_mixer.gain(FB_WET_CH, feedback);
    wet_dry_mixer.gain(WD_WET_CH, wet);
    wet_dry_mixer.gain(WD_DRY_CH, 1.0f - wet);
  }
  void View() {
    gfxPrint(0, 15, "Time: ");
    gfxPrint(delayMs, 3);
    gfxPrint("ms");
    gfxPrint(0, 25, "FB: ");
    gfxPrint(feedback, 3);
    gfxPrint(0, 35, "Wet: ");
    gfxPrint(wet, 3);
  }
  void OnButtonPress() {CursorToggle();}
  void OnEncoderMove(int direction) {}
  uint64_t OnDataRequest() {return 0;}
  void OnDataReceive(uint64_t data) {}

protected:
  void SetHelp() {}

private:
  float delayMs = 100.0f;
  float wet = 0.5f;
  float feedback = 0.5f;

  const uint8_t FB_DRY_CH = 0;
  const uint8_t FB_WET_CH = 1;
  const uint8_t WD_DRY_CH = 0;
  const uint8_t WD_WET_CH = 1;

  AudioEffectDelay delay;
  AudioMixer4 feedback_mixer;
  AudioConnection input_conn {input_stream, 0, feedback_mixer, FB_DRY_CH};
  AudioConnection delay_in{feedback_mixer, 0, delay, 0};
  AudioConnection feedback_out{delay, 0, feedback_mixer, FB_WET_CH};

  AudioMixer4 wet_dry_mixer;
  AudioConnection wet_conn{delay, 0, wet_dry_mixer, WD_WET_CH};
  AudioConnection dry_conn{input_stream, 0, wet_dry_mixer, WD_DRY_CH};
  AudioConnection output_conn{wet_dry_mixer, output_stream};
};
