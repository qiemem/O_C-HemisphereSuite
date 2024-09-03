#include "../tideslite.h"
#include "util/util_phase_extractor.h"

class EbbAndLfo : public HemisphereApplet {
public:
  enum EbbAndLfoCursor {
    LEVEL,
    FREQUENCY,
    CLOCKED,
    SLOPE_VAL,
    SHAPE_VAL,
    FOLD_VAL,
    OUTPUT_MODES,
    INPUT_MODES,
    ONESHOT_MODE,

    MAX_CURSOR = ONESHOT_MODE
  };

  const char *applet_name() { return "Ebb & LFO"; }

  void Start() {
    phase = 0;
    phase_extractor.Init();
  }

  void Controller() {
    if (knob_accel > (1 << 8))
      knob_accel--;

    if (Clock(1)) { // reset/retrigger
      phase = 0;
      oneshot_active = true;
    }

    int freq_div_mul = ratio;
    pitch_mod = pitch;
    bool got_clock = Clock(0);
    oneshot_active |= got_clock;

    // handle CV inputs
    slope_mod = slope * (65535 / 127);
    shape_mod = shape * (65535 / 127);
    fold_mod = fold * (32767 / 127);
    level_mod = level * 10;

    ForEachChannel(ch) {
      switch (cv_type(ch)) {
      case FREQ:
        if (ch == 0) {
          if (clocked) {
            freq_div_mul += semitones_to_div(SemitoneIn(ch));
          } else
            pitch_mod += In(ch);
        } else {
          Modulate(level_mod, ch, 0, 1000);
        }
        break;
      case SLOPE:
        Modulate(slope_mod, ch, 0, 65535);
        break;
      case SHAPE:
        shape_mod += Proportion(DetentedIn(ch), HEMISPHERE_MAX_INPUT_CV, 65535);
        while (shape_mod < 0)
          shape_mod += 65535;
        while (shape_mod > 65535)
          shape_mod -= 65535;
        break;
      case FOLD:
        Modulate(fold_mod, ch, 0, 32767);
        break;
      }
    }

    // TODO: Make oneshot and clocked mode more compatible so you can trigger a
    // shot with one trig and have it synced to clock on the other trig.
    if (oneshot_mode && !oneshot_active)
      return;

    uint32_t oldphase = phase;
    if (clocked) {
      phase = phase_extractor.Advance(got_clock, freq_div_mul);
    } else {
      uint32_t phase_increment = ComputePhaseIncrement(pitch_mod);
      phase += phase_increment;
    }

    // check for rollover and stop for one-shot mode
    if (phase < oldphase && oneshot_mode) {
      phase = 0;
      oneshot_active = false;
      Out(0, 0);
      Out(1, 0);
      return;
    }

    // COMPUTE
    int s = constrain(slope_mod, 0, 65535);
    ProcessSample(s, shape_mod, fold_mod, phase, sample);

    ForEachChannel(ch) {
      switch (output(ch)) {
      case UNIPOLAR:
        Out(ch, Proportion(sample.unipolar, 65535, HEMISPHERE_MAX_CV) *
                    level_mod / 1000);
        break;
      case BIPOLAR:
#ifdef VOR
        Out(ch, Proportion(sample.bipolar, 32767, 7680) * level_mod /
                    1000); // hardcoded at 5V for Plum Audio
#else
        Out(ch, Proportion(sample.bipolar, 32767, HEMISPHERE_MAX_CV / 2) *
                    level_mod / 1000);
#endif
        break;
      case EOA:
        GateOut(ch, sample.flags & FLAG_EOA);
        break;
      case EOR:
        GateOut(ch, sample.flags & FLAG_EOR);
        break;
      }
    }
  }

  void View() {
    ForEachChannel(ch) {
      int h = 17;
      int bottom = 32 + (h + 1) * ch;
      int last = bottom;
      for (int i = 0; i < 64; i++) {
        ProcessSample(slope_mod, shape_mod, fold_mod, 0xffffffff / 64 * i,
                      disp_sample);
        int next = 0;
        switch (output(ch)) {
        case UNIPOLAR:
          next = bottom - disp_sample.unipolar * h / 65535 * level_mod / 1000;
          break;
        case BIPOLAR:
          next = bottom -
                 (disp_sample.bipolar * level_mod / 1000 + 32767) * h / 65535;
          break;
        case EOA:
          next = bottom - ((disp_sample.flags & FLAG_EOA) ? h : 0);
          break;
        case EOR:
          next = bottom - ((disp_sample.flags & FLAG_EOR) ? h : 0);
          break;
        }
        if (i > 0)
          gfxLine(i - 1, last, i, next);
        last = next;
        // gfxPixel(i, 50 - disp_sample.unipolar * 35 / 65536);
      }
    }
    uint32_t p = phase / (0xffffffff / 64);
    gfxLine(p, 15, p, 50);

    const int param_y = 55;
    bool uses_cursor = false;
    switch (cursor) {
    case LEVEL:
      gfxPrint(0, param_y, "Level: ");
      gfxPrint(level);

      break;
    case FREQUENCY:
    case CLOCKED:
      uses_cursor = true;
      gfxStartCursor(2, param_y);
      if (clocked) {
        if (ratio >= 0) {
          gfxPrint("x");
          gfxPrint(ratio + 1);
        } else {
          gfxPrint("/");
          gfxPrint(-ratio + 1);
        }
      } else {
        gfxPrintFreq(pitch);
      }
      gfxEndCursor(cursor == FREQUENCY);

      gfxStartCursor(54, param_y);
      gfxPrintIcon(clocked ? CLOCK_ICON : VOCT_ICON);
      gfxEndCursor(cursor == CLOCKED);
      break;
    case SLOPE_VAL:
      gfxPrint(0, param_y, "Slope: ");
      gfxPrint(slope);
      break;
    case SHAPE_VAL:
      gfxPrint(0, param_y, "Shape: ");
      gfxPrint(shape);
      break;
    case FOLD_VAL:
      gfxPrint(0, param_y, "Fold: ");
      gfxPrint(fold);
      break;
    case OUTPUT_MODES:
      gfxPrint(0, param_y, OutputLabel(0));
      gfxPrint(":");
      gfxPrint(out_labels[output(0)]);
      gfxPrint(" ");
      gfxPrint(OutputLabel(1));
      gfxPrint(":");
      gfxPrint(out_labels[output(1)]);
      break;
    case INPUT_MODES:
      ForEachChannel(ch) {
        gfxIcon(0 + ch * 32, param_y, CV_ICON);
        gfxBitmap(8 + ch * 32, param_y, 3, ch ? SUB_TWO : SUP_ONE);
        gfxPrint(13 + ch * 32, param_y,
                 (ch == 1 && cv_type(ch) == FREQ) ? "Am"
                                                  : cv_labels[cv_type(ch)]);
      }
      break;
    case ONESHOT_MODE:
      gfxPrint(0, param_y, "Oneshot:");
      gfxIcon(54, param_y, oneshot_mode ? CHECK_ON_ICON : CHECK_OFF_ICON);
      break;
    }
    if (!uses_cursor && EditMode())
      gfxInvert(0, 55, 64, 9);
  }

  void OnButtonPress() {
    switch (cursor) {
    case ONESHOT_MODE:
      oneshot_mode = !oneshot_mode;
      break;
    case CLOCKED:
      clocked = !clocked;
      break;
    default:
      CursorToggle();
    }
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }

    switch (cursor) {
    case LEVEL: {
      level = constrain(level + direction, 0, 100);
      break;
    }
    case FREQUENCY: {
      if (clocked) {
        ratio = constrain(ratio + direction, -128, 127);
      } else {
        uint32_t old_pi = ComputePhaseIncrement(pitch);
        pitch += (knob_accel >> 8) * direction;
        while (ComputePhaseIncrement(pitch) == old_pi) {
          pitch += direction;
        }
      }
      break;
    }
    case SLOPE_VAL: {
      // slope += (knob_accel >> 4) * direction;
      slope = constrain(slope + direction, 0, 127);
      break;
    }
    case SHAPE_VAL: {
      shape += direction;
      while (shape < 0)
        shape += 128;
      while (shape > 127)
        shape -= 128;
      break;
    }
    case FOLD_VAL: {
      fold = constrain(fold + direction, 0, 127);
      break;
    }
    case OUTPUT_MODES: {
      out += direction;
      out %= 0b10000;
      break;
    }
    case INPUT_MODES:
      cv += direction;
      cv %= 0b10000;
      break;

    case ONESHOT_MODE:
      oneshot_mode = !oneshot_mode;
      break;
    }

    if (knob_accel < (1 << 13))
      knob_accel <<= 1;
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation{0, 16}, pitch + (1 << 15));
    Pack(data, PackLocation{16, 7}, slope);
    Pack(data, PackLocation{23, 7}, shape);
    Pack(data, PackLocation{30, 7}, fold);
    Pack(data, PackLocation{37, 4}, out);
    Pack(data, PackLocation{41, 4}, cv);
    Pack(data, PackLocation{45, 1}, oneshot_mode);
    Pack(data, PackLocation{46, 7}, level);
    Pack(data, PackLocation{53, 1}, clocked);
    Pack(data, PackLocation{54, 8}, ratio + 128);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    clocks_received = 0;
    pitch = Unpack(data, PackLocation{0, 16}) - (1 << 15);
    slope = Unpack(data, PackLocation{16, 7});
    shape = Unpack(data, PackLocation{23, 7});
    fold = Unpack(data, PackLocation{30, 7});
    out = Unpack(data, PackLocation{37, 4});
    cv = Unpack(data, PackLocation{41, 4});
    oneshot_mode = Unpack(data, PackLocation{45, 1});
    level = Unpack(data, PackLocation{46, 7});
    clocked = Unpack(data, PackLocation{53, 1});
    ratio = Unpack(data, PackLocation{54, 8}) - 128;
  }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = oneshot_mode ? "Retrig" : "Reset";
    help[HELP_CV1] = cv_labels[cv_type(0)];
    help[HELP_CV2] = cv_labels[cv_type(1)];
    help[HELP_OUT1] = out_labels[output(0)];
    help[HELP_OUT2] = out_labels[output(1)];
    help[HELP_EXTRA1] = "Enc: Select/Edit";
    help[HELP_EXTRA2] = "     Parameters";
    //                   "---------------------" <-- Extra text size guide
  }

private:
  enum Output {
    UNIPOLAR,
    BIPOLAR,
    EOA,
    EOR,
  };
  const char *const out_labels[4] = {"Un", "Bi", "Hi", "Lo"};

  enum CV {
    FREQ,
    SLOPE,
    SHAPE,
    FOLD,
  };
  const char *const cv_labels[4] = {"Hz", "Sl", "Sh", "Fo"};

  int cursor = 0;
  int16_t pitch = -3 * 12 * 128;
  int16_t ratio = 0;
  int slope = 64;
  int shape = 48; // triangle
  int fold = 0;
  int level = 100; // 0 to 100

  // actual values after CV mod
  int16_t pitch_mod;
  int slope_mod;
  int shape_mod;
  int fold_mod;
  int level_mod; // 0 to 1000 (higher precision for CV)

  bool oneshot_mode = 0;
  bool oneshot_active = 0;
  bool clocked = 0;

  PhaseExtractor<> phase_extractor;

  // Three semitones per div
  static const int16_t semitone_per_div = 3;
  static const int16_t pitch_per_div = 128 * semitone_per_div;

  // [-4,-2]=>-1, [-1,1]=>0, [2,4]=>1, etc
  int16_t semitones_to_div(int16_t semis) {
    return (semis + 32767) / 3 - 10922;
  }

  int clocks_received = 0;

  uint8_t out = 0b0001; // Unipolar on A, bipolar on B
  uint8_t cv = 0b0001;  // Freq on 1, shape on 2
  TidesLiteSample disp_sample;
  TidesLiteSample sample;

  int knob_accel = 1 << 8;

  uint32_t phase;

  Output output(int ch) { return (Output)((out >> ((1 - ch) * 2)) & 0b11); }

  CV cv_type(int ch) { return (CV)((cv >> ((1 - ch) * 2)) & 0b11); }

  void gfxPrintFreq(int16_t pitch) {
    uint32_t num = ComputePhaseIncrement(pitch);
    uint32_t denom = 0xffffffff / 16666;
    bool swap = num < denom;
    if (swap) {
      uint32_t t = num;
      num = denom;
      denom = t;
    }
    int int_part = num / denom;
    int digits = 0;
    if (int_part < 10)
      digits = 1;
    else if (int_part < 100)
      digits = 2;
    else if (int_part < 1000)
      digits = 3;
    else
      digits = 4;

    gfxPrint(int_part);
    gfxPrint(".");

    num %= denom;
    while (digits < 4) {
      num *= 10;
      gfxPrint(num / denom);
      num %= denom;
      digits++;
    }
    if (swap) {
      gfxPrint("s");
    } else {
      gfxPrint("Hz");
    }
  }
};
