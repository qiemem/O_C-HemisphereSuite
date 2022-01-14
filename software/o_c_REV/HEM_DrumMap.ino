// Copyright (c) 2021, Benjamin Rosenbach
//
// Based on Grids pattern generator, Copyright 2011 Émilie Gillet.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "grids_resources.h"
#include "packmacros.h"

#define HEM_DRUMMAP_PULSE_ANIMATION_TICKS 500
#define HEM_DRUMMAP_VALUE_ANIMATION_TICKS 16000
#define HEM_DRUMMAP_AUTO_RESET_TICKS 30000

class DrumMap : public HemisphereApplet {
public:

    const char* applet_name() {
        return "DrumMap";
    }

    void Start() {
        step = 0;
        last_clock = OC::CORE::ticks;
    }

    void Controller() {
        cv1 = Proportion(DetentedIn(0), HEMISPHERE_MAX_CV, 255);
        cv2 = Proportion(DetentedIn(1), HEMISPHERE_MAX_CV, 255);


        if (Clock(1)) Reset(); // Reset

        if (Clock(0)) {
            // generate randomness for each drum type on first step of the pattern
            if (step == 0) {
                for (int i = 0; i < 3; i++) {
                    randomness[i] = random(0, chaos_() >> 2);
                }
            }

            ForEachChannel(ch) {
                // accent on ch 1 will be for whatever part ch 0 is set to
                uint8_t part = (ch == 1 && mode[ch] == 3) ? mode[0] : mode[ch];
                int level = ReadDrumMap(step, part, x_(), y_());
                level = constrain(level + randomness[part], 0, 255);
                // use ch 0 fill if ch 1 is in accent mode
                uint8_t threshold = (ch == 1 && mode[ch] == 3) ? ~fill_(0) : ~fill_(ch);
                if (level > threshold) {
                    if (mode[ch] < 3) {
                        // normal part
                        ClockOut(ch);
                        pulse_animation[ch] = HEM_DRUMMAP_PULSE_ANIMATION_TICKS;
                    } else if (level > 192) {
                        // accent
                        ClockOut(ch);
                        pulse_animation[ch] = HEM_DRUMMAP_PULSE_ANIMATION_TICKS;
                    }
                }
            }

            // keep track of last clock for auto-reset
            last_clock = OC::CORE::ticks;
            // loop back to first step
            if (++step > 31) step = 0;
        }

        // animate pulses
        ForEachChannel(ch) {
            if (pulse_animation[ch] > 0) {
                pulse_animation[ch]--;
            }
        }

        // animate value changes
        if (value_animation > 0) {
          value_animation--;
        }

        // auto-reset after ~2 seconds of no clock
        if (OC::CORE::ticks - last_clock > HEM_DRUMMAP_AUTO_RESET_TICKS && step != 0) {
            Reset();
        }

    }

    void View() {
        gfxHeader(applet_name());
        DrawInterface();
    }

    void OnButtonPress() {
        if (++cursor > 7) cursor = 0;
        if (mode[1] > 2 && cursor == 3) cursor = 4;
    }

    void OnEncoderMove(int direction) {
        // modes
        if (cursor == 0) {
            mode[0] += direction;
            if (mode[0] > 2) mode[0] = 0;
            if (mode[0] < 0) mode[0] = 2;
        }
        if (cursor == 1) {
            mode[1] += direction;
            if (mode[1] > 3) mode[1] = 0;
            if (mode[1] < 0) mode[1] = 3;
        }
        // fill
        if (cursor == 2) fill[0] = constrain(fill[0] += direction, 0, max_fill);
        if (cursor == 3) fill[1] = constrain(fill[1] += direction, 0, max_fill);
        // x/y
        if (cursor == 4) x = constrain(x += direction, 0, max_xy);
        if (cursor == 5) y = constrain(y += direction, 0, max_xy);
        // chaos
        if (cursor == 6) chaos = constrain(chaos += direction, 0, max_chaos);
        // cv assign
        if (cursor == 7) {
          cv_mode += direction;
          if (cv_mode > 2) cv_mode = 0;
          if (cv_mode < 0) cv_mode = 2;
        }
    }

    #define PACK_SPEC(PACK_OP) \
      PACK_OP(fill[0], fill_bits) \
      PACK_OP(fill[1], fill_bits) \
      PACK_OP(x,       xy_bits) \
      PACK_OP(y,       xy_bits) \
      PACK_OP(chaos,   chaos_bits) \
      PACK_OP(mode[0], mode_bits) \
      PACK_OP(mode[1], mode_bits) \
      PACK_OP(cv_mode, cv_mode_bits)

    ON_DATA_REQUEST(PACK_SPEC)
    ON_DATA_RECEIVE(PACK_SPEC)

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock   2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "A=Part A  B=Part B";
        help[HEMISPHERE_HELP_ENCODER]  = "Params/Config";
        //                               "------------------" <-- Size Guide
    }

private:
    const uint8_t *MODE_ICONS[3] = {BD_ICON,SN_ICON,HH_ICON};
    const char *CV_MODE_NAMES[3] = {"FILL A/B", "X/Y", "FA/CHAOS"};
    uint8_t cursor = 0;
    uint8_t step;
    uint8_t randomness[3] = {0, 0, 0};
    int pulse_animation[2] = {0, 0};
    int value_animation = 0;
    uint32_t last_clock;

    // settings
    int8_t mode[2] = {0, 1};
    int fill[2] = {128, 128};
    int x = 0;
    int y = 0;
    int chaos = 0;
    int8_t cv_mode = 0; // 0 = Fill A/B, 1 = X/Y, 2 = Fill A/Chaos
    int cv1 = 0; // internal tracking of cv inputs
    int cv2 = 0;

    static const int mode_bits = 2;
    static const int fill_bits = 5;
    static const int xy_bits = 6;
    static const int chaos_bits = 4;
    static const int cv_mode_bits = 2;

    CHECK_PACK_SIZE(PACK_SPEC)
    #undef PACK_SPEC

    const int max_fill = (1 << fill_bits) - 1;
    const int max_xy = (1 << xy_bits) - 1;
    const int max_chaos = (1 << chaos_bits) - 1;

    inline int fill_(int i) const {
      int cv = 0;
      if (i == 0 && (cv_mode == 0 || cv_mode == 2)) {
        cv = cv1;
      } else if (i == 1 && cv_mode == 0) {
        cv = cv2;
      }
      int f = fill[i] * 255 / max_fill;
      return constrain(f + cv, 0, 255);
    }

    inline int x_() const { return x * 255 / max_xy; }
    inline int y_() const { return y * 255 / max_xy; }
    inline int chaos_() const { return chaos * 255 / max_chaos; }

    uint8_t ReadDrumMap(uint8_t step, uint8_t part, uint8_t x, uint8_t y) {
      uint8_t i = x >> 6;
      uint8_t j = y >> 6;
      const uint8_t* a_map = grids::drum_map[i][j];
      const uint8_t* b_map = grids::drum_map[i + 1][j];
      const uint8_t* c_map = grids::drum_map[i][j + 1];
      const uint8_t* d_map = grids::drum_map[i + 1][j + 1];
      uint8_t offset = (part * 32) + step;
      uint8_t a = a_map[offset];
      uint8_t b = b_map[offset];
      uint8_t c = c_map[offset];
      uint8_t d = d_map[offset];
      uint8_t quad_x = x << 2;
      uint8_t quad_y = y << 2;
      // return U8Mix(U8Mix(a, b, x << 2), U8Mix(c, d, x << 2), y << 2);
      // U8Mix returns b * x + a * (255 - x) >> 8
      uint8_t ab_fade = (b * quad_x + a * (255 - quad_x)) >> 8;
      uint8_t cd_fade = (d * quad_x + c * (255 - quad_x)) >> 8;
      return (cd_fade * quad_y + ab_fade * (255 - quad_y)) >> 8;
    }

    void DrawInterface() {
        // output selection
        gfxPrint(1,15,"A:");
        gfxIcon(14,14,MODE_ICONS[mode[0]]);
        gfxPrint(32,15,"B:");
        if (mode[1] == 3) {
            // accent
            gfxIcon(45,14,MODE_ICONS[mode[0]]);
            gfxPrint(53,15,">");
        } else {
            // standard
            gfxIcon(45,14,MODE_ICONS[mode[1]]);
        }
        // pulse animation per channel
         ForEachChannel(ch){
             if (pulse_animation[ch] > 0) {
                 gfxInvert(1+ch*32,15,8,8);
             }
         }

        // fill
        gfxPrint(1,25,"F");
        DrawKnobAt(9,25,20,fill_(0),cursor == 2);
        // don't show fill for channel b if it is an accent mode
        if (mode[1] < 3) {
            gfxPrint(32,25,"F");
            // add cv1 to fill_a value if cv1 mode is set to Fill A
            DrawKnobAt(40,25,20,fill_(1),cursor == 3);
        }

        // x & y
        gfxPrint(1,35,"X");
        DrawKnobAt(9,35,20,x_(),cursor == 4);
        gfxPrint(32,35,"Y");
        DrawKnobAt(40,35,20,y_(),cursor == 5);

        // chaos
        gfxPrint(1,45,"CHAOS");
        DrawKnobAt(32,45,28,chaos_(),cursor == 6);

        // cv input assignment
        gfxIcon(1,57,CV_ICON);
        gfxPrint(10,55,CV_MODE_NAMES[cv_mode]);

        // step count in header
        gfxPrint((step < 9 ? 49 : 43),2,step+1);

        // cursor for non-knobs
        if (cursor == 0) gfxCursor(14,23,16); // Part A
        if (cursor == 1) gfxCursor(45,23,16); // Part B
        if (cursor == 7) gfxCursor(10,63,50); // CV Assign

        // display value for knobs
        if (value_animation > 0 && cursor >= 2 && cursor <= 6) {
          gfxRect(1, 54, 60, 10);
          gfxInvert(1, 54, 60, 10);
          int val = 0;
          switch (cursor) {
            case 2:
              val = fill_(0);
              break;
            case 3:
              val = fill_(1);
              break;
            case 4:
              val = x_();
              break;
            case 5:
              val = y_();
              break;
            case 6:
              val = chaos_();
              break;
          }
          int xPos = 27;
          if (val > 99) {
            xPos = 21;
          } else if (val > 9) {
            xPos = 24;
          }
          gfxPrint(xPos, 55, val);
          gfxInvert(1, 54, 60, 10);
        }
    }

    void DrawKnobAt(byte x, byte y, byte len, byte value, bool is_cursor) {
        byte w = Proportion(value, 255, len);
        byte p = is_cursor ? 1 : 3;
        gfxDottedLine(x, y + 4, x + len, y + 4, p);
        gfxRect(x + w, y, 2, 7);
    }

    void Reset() {
        step = 0;
    }
};

DrumMap DrumMap_instance[2];

void DrumMap_Start(bool hemisphere) {DrumMap_instance[hemisphere].BaseStart(hemisphere);}
void DrumMap_Controller(bool hemisphere, bool forwarding) {DrumMap_instance[hemisphere].BaseController(forwarding);}
void DrumMap_View(bool hemisphere) {DrumMap_instance[hemisphere].BaseView();}
void DrumMap_OnButtonPress(bool hemisphere) {DrumMap_instance[hemisphere].OnButtonPress();}
void DrumMap_OnEncoderMove(bool hemisphere, int direction) {DrumMap_instance[hemisphere].OnEncoderMove(direction);}
void DrumMap_ToggleHelpScreen(bool hemisphere) {DrumMap_instance[hemisphere].HelpScreen();}
uint32_t DrumMap_OnDataRequest(bool hemisphere) {return DrumMap_instance[hemisphere].OnDataRequest();}
void DrumMap_OnDataReceive(bool hemisphere, uint32_t data) {DrumMap_instance[hemisphere].OnDataReceive(data);}
