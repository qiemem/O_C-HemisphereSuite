#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)

#include "extern/dspinst.h"
#include "AudioSetup.h"
#include "OC_ADC.h"
#include "OC_DAC.h"
#include "HSUtils.h"
#include "HSicons.h"
#include "OC_strings.h"
#include "HSClockManager.h"

// Use the web GUI tool as a guide: https://www.pjrc.com/teensy/gui/

// GUItool: begin automatically generated code
AudioPlaySdWav           wavplayer1;     //xy=108,191
AudioSynthWaveform       waveform2;      //xy=128,321
AudioSynthWaveform       waveform1;      //xy=129,139
AudioInputI2S2           i2s1;           //xy=129,245
AudioSynthWaveformDc     dc2;            //xy=376.66668701171875,359.4444580078125
AudioMixer4              mixer1;         //xy=380.4444580078125,164.3333282470703
AudioMixer4              mixer2;         //xy=380.3333740234375,292.6666564941406
AudioSynthWaveformDc     dc1;            //xy=381.4444580078125,222.88888549804688
AudioEffectWaveFolder    wavefolder2;    //xy=571.4444580078125,334.888916015625
AudioEffectWaveFolder    wavefolder1;    //xy=575.2222290039062,217.44442749023438
AudioMixer4              mixer4;         //xy=743.000244140625,311.77783203125
AudioMixer4              mixer3;         //xy=750.4443359375,181.3333282470703
AudioAmplifier           amp1;           //xy=818,81
AudioAmplifier           amp2;           //xy=884,357
AudioFilterStateVariable svfilter1;        //xy=964,248
AudioFilterLadder        ladder1;        //xy=970,96.88888549804688
AudioMixer4              mixer6;         //xy=1106,310
AudioMixer4              mixer5;         //xy=1113,156
AudioEffectDynamics      complimiter[2];
AudioOutputI2S2          i2s2;           //xy=1270.2222900390625,227.88890075683594

AudioConnection          patchCord1(wavplayer1, 0, mixer5, 2);
AudioConnection          patchCord2(wavplayer1, 1, mixer6, 2);
AudioConnection          patchCord3(waveform2, 0, mixer2, 1);
AudioConnection          patchCord4(waveform1, 0, mixer1, 1);
AudioConnection          patchCord5(i2s1, 0, mixer1, 0);
AudioConnection          patchCord6(i2s1, 1, mixer2, 0);
AudioConnection          patchCord7(dc2, 0, wavefolder2, 1);
AudioConnection          patchCord8(mixer1, 0, wavefolder1, 0);
AudioConnection          patchCord9(mixer1, 0, mixer3, 0);
AudioConnection          patchCord10(mixer2, 0, wavefolder2, 0);
AudioConnection          patchCord11(mixer2, 0, mixer4, 0);
AudioConnection          patchCord12(dc1, 0, wavefolder1, 1);
AudioConnection          patchCord13(wavefolder2, 0, mixer4, 3);
AudioConnection          patchCord14(wavefolder1, 0, mixer3, 3);
AudioConnection          patchCord15(mixer4, 0, mixer6, 3);
AudioConnection          patchCord16(mixer4, amp2);
AudioConnection          patchCord17(mixer3, 0, mixer5, 3);
AudioConnection          patchCord18(mixer3, amp1);
AudioConnection          patchCord19(amp1, 0, ladder1, 0);
AudioConnection          patchCord20(amp2, 0, svfilter1, 0);
AudioConnection          patchCord21(svfilter1, 0, mixer6, 0);
AudioConnection          patchCord24(svfilter1, 0, mixer5, 1);
AudioConnection          patchCord22(ladder1, 0, mixer5, 0);
AudioConnection          patchCord26(ladder1, 0, mixer6, 1);

AudioConnection          patchCord25(mixer5, 0, complimiter[0], 0);
AudioConnection          patchCord23(mixer6, 0, complimiter[1], 0);
AudioConnection          patchCord27(complimiter[0], 0, i2s2, 0);
AudioConnection          patchCord28(complimiter[1], 0, i2s2, 1);

// GUItool: end automatically generated code

// Some time ago...
// These reverbs were fed from the final outputs and looped back into BOTH mixers...
// mixer3 and mixer4 controlled the balance:
// 0 - dry
// 1 - this reverb
// 2 - other reverb
// 3 - wavefold
//AudioEffectFreeverb      freeverb2;      //xy=1132.5553283691406,255.11116409301758
//AudioEffectFreeverb      freeverb1;      //xy=1135.6664810180664,188.5555362701416
//AudioConnection          patchCord20(mixer4, freeverb2);
//AudioConnection          patchCord22(mixer3, freeverb1);
//AudioConnection          patchCord23(freeverb2, 0, mixer4, 1);
//AudioConnection          patchCord24(freeverb2, 0, mixer3, 2);
//AudioConnection          patchCord25(freeverb1, 0, mixer3, 1);
//AudioConnection          patchCord26(freeverb1, 0, mixer4, 2);

// Notes:
//
// amp1 and amp2 are beginning of chain, for pre-filter attenuation
// dc1 and dc2 are control signals for modulating the wavefold amount.
//
// Every mixer input is a VCA.
// VCA modulation could control all of them.
//

namespace OC {
  namespace AudioDSP {

    const char * const mode_names[MODE_COUNT] = {
      "Off", "VCA", "VCF", "FOLD", "File"
    };

    /* Mod Targets:
      AMP_LEVEL
      FILTER_CUTOFF,
      FILTER_RESONANCE,
      WAVEFOLD_MOD,
      REVERB_LEVEL,
      REVERB_SIZE,
      REVERB_DAMP,
     */
    int mod_map[2][TARGET_COUNT] = {
      { -1, -1, -1, -1, -1, -1, -1 },
      { -1, -1, -1, -1, -1, -1, -1 },
    };
    float bias[2][TARGET_COUNT];
    ChannelMode audio_cursor[2] = { PASSTHRU, PASSTHRU };
    bool isEditing[2] = { false, false };

    float amplevel[2] = { 1.0, 1.0 };
    float foldamt[2] = { 0.0, 0.0 };

    bool filter_enabled[2];
    bool wavplayer_available = false;
    uint8_t wavplayer_select[2] = { 1, 2 };

    void BypassFilter(int ch) {
      if (ch == 0) {
        mixer5.gain(0, 0.0); // VCF
        mixer6.gain(1, 0.0); // VCF

        mixer5.gain(3, 0.9); // Dry
      } else {
        mixer6.gain(0, 0.0); // VCF
        mixer5.gain(1, 0.0); // VCF

        mixer6.gain(3, 0.9); // Dry
      }
      filter_enabled[ch] = false;
    }

    void EnableFilter(int ch) {
      if (ch == 0) {
        mixer5.gain(0, 1.0); // VCF
        mixer6.gain(1, 1.0); // VCF

        mixer5.gain(3, 0.0); // Dry
      } else {
        mixer6.gain(0, 1.0); // VCF
        mixer5.gain(1, 1.0); // VCF

        mixer6.gain(3, 0.0); // Dry
      }
      filter_enabled[ch] = true;
    }

    void ModFilter(int ch, int cv) {
      // quartertones squared
      // 1 Volt is 576 Hz
      // 2V = 2304 Hz
      // 3V = 5184 Hz
      // 4V = 9216 Hz
      float freq = abs(cv) / 64 + bias[ch][FILTER_CUTOFF];
      freq *= freq;

      if (ch == 0)
        ladder1.frequency(freq);
      else
        svfilter1.frequency(freq);
    }


    void Wavefold(int ch, int cv) {
      foldamt[ch] = (float)cv / MAX_CV + bias[ch][WAVEFOLD_MOD];
      if (ch == 0) {
        dc1.amplitude(foldamt[ch]);
        mixer3.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
        mixer3.gain(3, foldamt[ch] * 0.9);
      } else {
        dc2.amplitude(foldamt[ch]);
        mixer4.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
        mixer4.gain(3, foldamt[ch] * 0.9);
      }
    }

    void AmpLevel(int ch, int cv) {
      amplevel[ch] = (float)cv / MAX_CV + bias[ch][AMP_LEVEL];
      if (ch == 0)
        mixer3.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
      else
        mixer4.gain(0, amplevel[ch] * (1.0 - abs(foldamt[ch])));
    }

    bool FileIsPlaying() {
      return wavplayer1.isPlaying();
    }
    void ToggleFilePlayer(int ch = 0) {
      if (wavplayer1.isPlaying()) {
        wavplayer1.stop();
      } else if (wavplayer_available) {
        char filename[] = "000.WAV";
        filename[2] += wavplayer_select[ch];
        wavplayer1.play(filename);
      }
    }
    void PlayFile1() { ToggleFilePlayer(0); }
    void PlayFile2() { ToggleFilePlayer(1); }
    void ChangeToFile(int ch, int select) {
      wavplayer_select[ch] = (uint8_t)constrain(select, 0, 9);
      if (wavplayer1.isPlaying()) {
        char filename[] = "000.WAV";
        filename[2] += wavplayer_select[ch];
        wavplayer1.play(filename);
      }
    }
    uint8_t GetFileNum(int ch) {
      return wavplayer_select[ch];
    }
    uint32_t GetFileTime(int ch) {
      return wavplayer1.positionMillis();
    }

    // Designated Integration Functions
    // ----- called from setup() in Main.cpp
    void Init() {
      AudioMemory(128);

      // --Wavefolders
      dc1.amplitude(0.00);
      dc2.amplitude(0.00);
      mixer3.gain(3, 0.9);
      mixer4.gain(3, 0.9);

      // --Filters
      amp1.gain(0.85); // attenuate before ladder filter
      amp2.gain(0.85); // attenuate before svfilter1
      svfilter1.resonance(1.05);
      ladder1.resonance(0.65);
      BypassFilter(0);
      BypassFilter(1);

      // -- SD card WAV player
      mixer5.gain(2, 0.9);
      mixer6.gain(2, 0.9);
      wavplayer_available = SD.begin(BUILTIN_SDCARD);
      if (!wavplayer_available) {
        Serial.println("Unable to access the SD card");
      }

      // --Reverbs
      /*
      freeverb1.roomsize(0.7);
      freeverb1.damping(0.5);
      mixer3.gain(1, 0.08); // verb1
      mixer3.gain(2, 0.05); // verb2

      freeverb2.roomsize(0.8);
      freeverb2.damping(0.6);
      mixer4.gain(1, 0.08); // verb2
      mixer4.gain(2, 0.05); // verb1
      */
      
      // -- Master Compressor / Limiter
      for (int ch = 0; ch < 2; ++ch) {
        complimiter[ch].compression(-20.0f);
        complimiter[ch].makeupGain(0.0);
        // default gate() and limit() settings
      }
    }

    // ----- called from Controller thread
    void Process(const int *values) {
      for (int i = 0; i < 2; ++i) {

        if (mod_map[i][AMP_LEVEL] >= 0)
          AmpLevel(i, values[mod_map[i][AMP_LEVEL]]);

        if (mod_map[i][WAVEFOLD_MOD] >= 0)
          Wavefold(i, values[mod_map[i][WAVEFOLD_MOD]]);

        if (mod_map[i][FILTER_CUTOFF] >= 0)
          ModFilter(i, values[mod_map[i][FILTER_CUTOFF]]);

      }
    }

    void AudioMenuAdjust(int ch, int direction) {
      if (!isEditing[ch]) {
        audio_cursor[ch] = (ChannelMode)constrain(audio_cursor[ch] + direction, 0, MODE_COUNT - 1);
        return;
      }

      int mod_target = AMP_LEVEL;
      switch (audio_cursor[ch]) {
        case PASSTHRU: {
          return;
          break;
        }
        case VCF_MODE:
          mod_target = FILTER_CUTOFF;
          break;
        case WAVEFOLDER:
          mod_target = WAVEFOLD_MOD;
          break;
        case WAV_PLAYER:
          ChangeToFile(ch, wavplayer_select[ch] + direction);

          return; // no other mapping to change
          break;
        default: break;
      }

      // congrats you get to switch one of the input map mod sources
      int &targ = mod_map[ch][mod_target];
      targ = constrain(targ + direction + 1, 0, ADC_CHANNEL_LAST + DAC_CHANNEL_LAST) - 1;

      if (audio_cursor[ch] == VCA_MODE && targ < 0)
        AmpLevel(ch, MAX_CV);
    }

    void AudioSetupAuxButton(int ch) {
      switch (audio_cursor[ch]) {
        case PASSTHRU:
          Wavefold(ch, 0);
          AmpLevel(ch, MAX_CV);
          BypassFilter(ch);
          break;

        case WAV_PLAYER:
          if (HS::clock_m.IsRunning()) {
            HS::clock_m.BeatSync( ch ? &PlayFile2 : &PlayFile1 );
          } else
            ToggleFilePlayer(ch);
          break;

        case VCF_MODE:
          Wavefold(ch, 0);
          AmpLevel(ch, MAX_CV);
          filter_enabled[ch] ? BypassFilter(ch) : EnableFilter(ch);
          break;

        default: break;
      }
      //isEditing[ch] = false;
    }

  } // AudioDSP namespace
} // OC namespace

#endif
