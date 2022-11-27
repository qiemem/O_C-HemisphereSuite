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

    ForEachChannel(ch) {
      switch (output(ch)) {
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

    if (knob_accel > (1 << 8))
      knob_accel--;
  }

  void View() {
    gfxHeader(applet_name());

    ForEachChannel(ch) {
      int h = 17;
      int bottom = 32 + (h + 1) * ch;
      int last = bottom;
      for (int i = 0; i < 64; i++) {
        ProcessSample(slope * 65535 / 127, shape * 65535 / 127,
                      fold * 32767 / 127, 0xffffffff / 64 * i, disp_sample);
        int next = 0;
        switch (output(ch)) {
        case UNIPOLAR:
          next = bottom - disp_sample.unipolar * h / 65535;
          break;
        case BIPOLAR:
          next = bottom - (disp_sample.bipolar + 32767) * h / 65535;
          break;
        case EOA:
          next = bottom - ((disp_sample.flags & FLAG_EOA) ? h : 0);
          break;
        case EOR:
          next = bottom - ((disp_sample.flags & FLAG_EOR) ? h : 0);
          break;
        }
        if (i > 0) gfxLine(i - 1, last, i, next);
        last = next;
        // gfxPixel(i, 50 - disp_sample.unipolar * 35 / 65536);
      }
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
      gfxPrint(0, 55, "Shape: ");
      gfxPrint(shape);
      break;
    case 3:
      gfxPrint(0, 55, "Fold: ");
      gfxPrint(fold);
      break;
    case 4:
      gfxPrint(0, 55, hemisphere == 0 ? "A: " : "C: ");
      gfxPrint(out_labels[output(0)]);
      gfxPrint(hemisphere == 0 ? " B: " : " D: ");
      gfxPrint(out_labels[output(1)]);
      break;
    }
  }

  void SetHelp() {}

  void OnButtonPress() {
    cursor++;
    cursor %= 5;
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
      out += direction;
      out %= 0b10000;
    }
    }
    if (knob_accel < (1 << 13))
      knob_accel <<= 1;
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation { 0, 16 }, pitch + (1 << 15));
    Pack(data, PackLocation { 16, 7 }, slope);
    Pack(data, PackLocation { 23, 7 }, shape + 128);
    Pack(data, PackLocation { 30, 7 }, fold);
    Pack(data, PackLocation { 37, 4 }, out);
    Pack(data, PackLocation { 41, 4 }, cv);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    pitch = Unpack(data, PackLocation { 0, 16 }) - (1 << 15);
    slope = Unpack(data, PackLocation { 16, 7 });
    shape = Unpack(data, PackLocation { 23, 7 }) - 128;
    fold = Unpack(data, PackLocation { 30, 7 });
    out = Unpack(data, PackLocation { 37, 4 });
    cv = Unpack(data, PackLocation { 41, 4 });
  }

private:

  enum Output {
    UNIPOLAR,
    BIPOLAR,
    EOA,
    EOR,
  };
  const char* out_labels[4] = {"Un", "Bi", "Hi", "Lo"};

  enum CV {
    FREQ,
    SLOPE,
    SHAPE,
    FOLD,
  };
  const char* cv_labels[4] = {"Hz", "Sl", "Sh", "Fo"};

  int cursor = 0;
  int16_t pitch = -3 * 12 * 128;
  int slope = 64;
  int shape = 48; // triangle
  int fold = 0;

  uint8_t out = 0b0001; // Unipolar on A, bipolar on B
  uint8_t cv = 0b0001; // Freq on 1, shape on 2
  TidesLiteSample disp_sample;
  TidesLiteSample sample;
  bool eoa_reached = false;

  int knob_accel = 1 << 8;

  uint32_t phase;

  Output output(int ch) {
    return (Output) ((out >> ((1 - ch) * 2)) & 0b11);
  }

  CV cv_type(int ch) {
    return (CV) ((out >> ((1 - ch) * 2)) & 0b11);
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