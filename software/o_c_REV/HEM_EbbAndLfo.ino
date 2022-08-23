#include "resources/tideslite.h"

class EbbAndLfo : public HemisphereApplet {
public:
  const char *applet_name() { return "Ebb & LFO"; }

  void Start() { phase = 0; }

  void Controller() {
    uint32_t phase_increment = ComputePhaseIncrement(pitch + In(0));
    phase += phase_increment;
    int slope_cv = In(1) * 32768 / HEMISPHERE_3V_CV;
    int s = constrain(slope * 65535 / 127 + slope_cv, 0, 65535);
    ProcessSample(s, shape * 65535 / 127, fold * 32767 / 127, phase, sample);
    if (phase < phase_increment) {
      eoa_reached = false;
    } else {
      eoa_reached = eoa_reached || (sample.flags & FLAG_EOA);
    }

    output(0, (Output) out_a);
    output(1, (Output) out_b);

    if (knob_accel > (1 << 8))
      knob_accel--;
  }

  void View() {
    gfxHeader(applet_name());

    int last = 50;
    for (int i = 1; i < 64; i++) {
      ProcessSample(slope * 65535 / 127, shape * 65535 / 127,
                    fold * 32767 / 127, 0xffffffff / 64 * i, disp_sample);
      int next = 50 - disp_sample.unipolar * 35 / 65535;
      gfxLine(i - 1, last, i, next);
      last = next;
      // gfxPixel(i, 50 - disp_sample.unipolar * 35 / 65536);
    }
    uint32_t p = phase / (0xffffffff / 64);
    gfxLine(p, 15, p, 50);

    switch (cursor) {
    case 0:
      // gfxPrint(0, 55, "Frq:");
      gfxPos(0, 55);
      gfxPrintFreq(pitch);
      break;
    case 1:
      gfxPrint(0, 55, "Slope: ");
      gfxPrint(slope);
      break;
    case 2:
      gfxPrint(0, 55, "Att: ");
      gfxPrint(shape);
      break;
    case 3:
      gfxPrint(0, 55, "Fold: ");
      gfxPrint(fold);
      break;
    case 4:
      gfxPrint(0, 55, "OutA: ");
      gfxPrint(out_labels[out_a]);
      break;
    case 5:
      gfxPrint(0, 55, "OutB: ");
      gfxPrint(out_labels[out_b]);
      break;
    }
  }

  void SetHelp() {}

  void OnButtonPress() {
    cursor++;
    cursor %= 6;
  }

  void OnEncoderMove(int direction) {
    switch (cursor) {
    case 0: {
      uint32_t old_pi = ComputePhaseIncrement(pitch);
      pitch += (knob_accel >> 8) * direction;
      while (ComputePhaseIncrement(pitch) == old_pi) {
        pitch += direction;
      }
      break;
    }
    case 1: {
      // slope += (knob_accel >> 4) * direction;
      slope = constrain(slope + direction, 0, 127);
      break;
    }
    case 2: {
      shape += direction;
      shape %= 128;
      break;
    }
    case 3: {
      fold = constrain(fold + direction, 0, 127);
      break;
    }
    case 4: {
      out_a += direction;
      out_a %= 4;
    }
    case 5: {
      out_b += direction;
      out_b %= 4;
    }
    }
    if (knob_accel < (1 << 13))
      knob_accel <<= 1;
  }

  uint64_t OnDataRequest() { return 0; }

  void OnDataReceive(uint64_t data) {}

private:

  enum Output {
    UNIPOLAR,
    BIPOLAR,
    EOA,
    EOR,
  };

  int cursor;
  int16_t pitch;
  int slope = 64;
  int shape = 32;
  int fold = 0;
  const char* out_labels[4] = {"Uni", "Bi", "High", "Low"};
  uint8_t out_a = UNIPOLAR;
  uint8_t out_b = BIPOLAR;
  TidesLiteSample disp_sample;
  TidesLiteSample sample;
  bool eoa_reached = false;

  int knob_accel = 1 << 8;

  uint32_t phase;

  void output(int ch, Output out) {
    switch (out) {
    case UNIPOLAR:
      Out(ch, Proportion(sample.unipolar, 65535, HEMISPHERE_MAX_CV));
      break;
    case BIPOLAR:
      Out(ch, Proportion(sample.bipolar, 32767, HEMISPHERE_MAX_CV / 2));
      break;
    case EOA:
      GateOut(ch, eoa_reached);
      break;
    case EOR:
      GateOut(ch, !eoa_reached);
      break;
    }
  }


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
////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to EbbAndLfo,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
EbbAndLfo EbbAndLfo_instance[2];

void EbbAndLfo_Start(bool hemisphere) {
  EbbAndLfo_instance[hemisphere].BaseStart(hemisphere);
}

void EbbAndLfo_Controller(bool hemisphere, bool forwarding) {
  EbbAndLfo_instance[hemisphere].BaseController(forwarding);
}

void EbbAndLfo_View(bool hemisphere) {
  EbbAndLfo_instance[hemisphere].BaseView();
}

void EbbAndLfo_OnButtonPress(bool hemisphere) {
  EbbAndLfo_instance[hemisphere].OnButtonPress();
}

void EbbAndLfo_OnEncoderMove(bool hemisphere, int direction) {
  EbbAndLfo_instance[hemisphere].OnEncoderMove(direction);
}

void EbbAndLfo_ToggleHelpScreen(bool hemisphere) {
  EbbAndLfo_instance[hemisphere].HelpScreen();
}

uint64_t EbbAndLfo_OnDataRequest(bool hemisphere) {
  return EbbAndLfo_instance[hemisphere].OnDataRequest();
}

void EbbAndLfo_OnDataReceive(bool hemisphere, uint64_t data) {
  EbbAndLfo_instance[hemisphere].OnDataReceive(data);
}