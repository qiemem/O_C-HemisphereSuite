// Copyright (c) 2018, Jason Justian
//
// Bjorklund pattern filter, Copyright (c) 2016 Tim Churches
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

#include "bjorklund.h"

const int NUM_PARAMS = 3;
const int PARAM_SIZE = 5;
const int OUTER_RADIUS = 24;
const int INNER_RADIUS = 16;

class AnnularFusion : public HemisphereApplet {
public:

    const char* applet_name() {
        return "AnnularFu";
    }

    void Start() {
        ForEachChannel(ch)
        {
            length[ch] = 16;
            beats[ch] = 4 + (ch * 4);
            pattern[ch] = EuclideanPattern(length[ch], beats[ch], 0);;
        }
        step = 0;
        last_clock = OC::CORE::ticks;
    }

    void Controller() {
        if (Clock(1)) step = 0; // Reset

        ForEachChannel(ch) {
            int rotation = Proportion(DetentedIn(ch), HEMISPHERE_MAX_CV, length[ch]);
            // Store the pattern for display
            pattern[ch] = EuclideanPattern(length[ch], beats[ch], rotation + offset[ch]);
        }


        // Advance both rings
        if (Clock(0)) {
            last_clock = OC::CORE::ticks;
            ForEachChannel(ch)
            {

                int sb = step % length[ch];
                if ((pattern[ch] >> sb) & 0x01) {
                    ClockOut(ch);
                }
            }

            // Plan for the thing to run forever and ever
            if (++step >= length[0] * length[1]) step = 0;
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawSteps();
        DrawEditor();
    }

    void OnButtonPress() {
        if (++cursor > 5) cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        int ch = cursor < NUM_PARAMS ? 0 : 1;
        int f = cursor - (ch * NUM_PARAMS); // Cursor function
        if (f == 0) {
            length[ch] = constrain(length[ch] + direction, 3, 32);
            if (beats[ch] > length[ch]) beats[ch] = length[ch];
            if (offset[ch] > length[ch]) offset[ch] = length[ch];
        }
        if (f == 1) {
            beats[ch] = constrain(beats[ch] + direction, 1, length[ch]);
        }
        if (f == 2) {
            offset[ch] = constrain(offset[ch] + direction, 0, length[ch] - 1);
        }
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0 * PARAM_SIZE, PARAM_SIZE}, length[0] - 1);
        Pack(data, PackLocation {1 * PARAM_SIZE, PARAM_SIZE}, beats[0] - 1);
        Pack(data, PackLocation {2 * PARAM_SIZE, PARAM_SIZE}, length[1] - 1);
        Pack(data, PackLocation {3 * PARAM_SIZE, PARAM_SIZE}, beats[1] - 1);
        Pack(data, PackLocation {4 * PARAM_SIZE, PARAM_SIZE}, offset[0]);
        Pack(data, PackLocation {5 * PARAM_SIZE, PARAM_SIZE}, offset[1]);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        length[0] = Unpack(data, PackLocation {0 * PARAM_SIZE, PARAM_SIZE}) + 1;
        beats[0]  = Unpack(data, PackLocation {1 * PARAM_SIZE, PARAM_SIZE}) + 1;
        length[1] = Unpack(data, PackLocation {2 * PARAM_SIZE, PARAM_SIZE}) + 1;
        beats[1]  = Unpack(data, PackLocation {3 * PARAM_SIZE, PARAM_SIZE}) + 1;
        offset[0] = Unpack(data, PackLocation {4 * PARAM_SIZE, PARAM_SIZE});
        offset[1] = Unpack(data, PackLocation {5 * PARAM_SIZE, PARAM_SIZE});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "Rotate 1=Ch1 2=Ch2";
        help[HEMISPHERE_HELP_OUTS]     = "Clock A=Ch1 B=Ch2";
        help[HEMISPHERE_HELP_ENCODER]  = "Len/Hits/Rot Ch1,2";
        //                               "------------------" <-- Size Guide
    }

private:
    int step;
    int cursor = 0; // Ch1: 0=Length, 1=Hits; Ch2: 2=Length 3=Hits
    uint32_t pattern[2];
    int last_clock;

    // Settings
    int length[2];
    int beats[2];
    int offset[2];

    void DrawSteps() {
        //int spacing = 1;
        gfxLine(0, 45, 63, 45);
        gfxLine(0, 62, 63, 62);
        gfxLine(0, 53, 63, 53);
        gfxLine(0, 54, 63, 54);
        ForEachChannel(ch) {
            for (int i = 0; i < 16; i++) {
                if ((pattern[ch] >> ((i + step) % length[ch])) & 0x1) {
                    gfxRect(4 * i + 1, 48 + 9 * ch, 3, 3);
                    //gfxLine(4 * i + 2, 47 + 9 * ch, 4 * i + 2, 47 + 9 * ch + 4);
                } else {
                    gfxPixel(4 * i + 2, 47 + 9 * ch + 2);
                }

                if ((i + step) % length[ch] == 0) {
                    //gfxLine(4 * i, 46 + 9 * ch, 4 * i, 52 + 9 * ch);
                    gfxLine(4 * i, 46 + 9 * ch, 4 * i, 46 + 9 * ch + 1);
                    gfxLine(4 * i, 52 + 9 * ch - 1, 4 * i, 52 + 9 * ch);
                }
            }
        }
    }

    void DrawEditor() {
        int spacing = 25;

        gfxBitmap(1 + 0 * spacing, 15, 8, LOOP_ICON);
        gfxBitmap(1 + 1 * spacing, 15, 8, X_NOTE_ICON);
        gfxBitmap(1 + 2 * spacing, 15, 8, LEFT_RIGHT_ICON);

        ForEachChannel (ch) {
            int y = 15 + 10 * (ch + 1);
            gfxPrint(1 + 0 * spacing, y, length[ch]);
            if (cursor == 0 + ch * NUM_PARAMS) gfxCursor(1 + 0 * spacing, y + 7, 12);

            gfxPrint(1 + 1 * spacing, y, beats[ch]);
            if (cursor == 1 + ch * NUM_PARAMS) gfxCursor(1 + 1 * spacing, y + 7, 12);

            gfxPrint(1 + 2 * spacing, y, offset[ch]);
            if (cursor == 2 + ch * NUM_PARAMS) gfxCursor(1 + 2 * spacing, y + 7, 12);
        }

    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to AnnularFusion,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
AnnularFusion AnnularFusion_instance[2];

void AnnularFusion_Start(bool hemisphere) {
    AnnularFusion_instance[hemisphere].BaseStart(hemisphere);
}

void AnnularFusion_Controller(bool hemisphere, bool forwarding) {
    AnnularFusion_instance[hemisphere].BaseController(forwarding);
}

void AnnularFusion_View(bool hemisphere) {
    AnnularFusion_instance[hemisphere].BaseView();
}

void AnnularFusion_OnButtonPress(bool hemisphere) {
    AnnularFusion_instance[hemisphere].OnButtonPress();
}

void AnnularFusion_OnEncoderMove(bool hemisphere, int direction) {
    AnnularFusion_instance[hemisphere].OnEncoderMove(direction);
}

void AnnularFusion_ToggleHelpScreen(bool hemisphere) {
    AnnularFusion_instance[hemisphere].HelpScreen();
}

uint32_t AnnularFusion_OnDataRequest(bool hemisphere) {
    return AnnularFusion_instance[hemisphere].OnDataRequest();
}

void AnnularFusion_OnDataReceive(bool hemisphere, uint32_t data) {
    AnnularFusion_instance[hemisphere].OnDataReceive(data);
}
