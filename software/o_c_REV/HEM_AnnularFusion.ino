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
#define AF_DISPLAY_TIMEOUT 330000

struct AFStepCoord {
    uint8_t x;
    uint8_t y;
};

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
        display_timeout = AF_DISPLAY_TIMEOUT;
        ForEachChannel(ch)
        {
            length[ch] = 16;
            beats[ch] = 4 + (ch * 4);
            pattern[ch] = EuclideanPattern(length[ch], beats[ch], 0);;
        }
        step = 0;
        SetDisplayPositions(0);
        SetDisplayPositions(1);
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
        if (display_timeout > 0) --display_timeout;
    }

    void View() {
        gfxHeader(applet_name());
        DrawSteps();
        //if (display_timeout > 0) DrawEditor();
        DrawEditor();
    }

    void OnButtonPress() {
        display_timeout = AF_DISPLAY_TIMEOUT;
        if (++cursor > 5) cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        display_timeout = AF_DISPLAY_TIMEOUT;
        int ch = cursor < NUM_PARAMS ? 0 : 1;
        int f = cursor - (ch * NUM_PARAMS); // Cursor function
        if (f == 0) {
            length[ch] = constrain(length[ch] + direction, 3, 32);
            if (beats[ch] > length[ch]) beats[ch] = length[ch];
            if (offset[ch] > length[ch]) offset[ch] = length[ch];
            SetDisplayPositions(ch);
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
        SetDisplayPositions(0);
        SetDisplayPositions(1);
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
    AFStepCoord disp_coord[2][32];
    uint32_t pattern[2];
    int last_clock;
    uint32_t display_timeout;

    // Settings
    int length[2];
    int beats[2];
    int offset[2];

    void DrawSteps() {
        //int spacing = 1;
        ForEachChannel(ch) {
            for (int i=0; i < 32; i++) {
                if ((pattern[ch] >> ((i + step) % length[ch])) & 0x1) {
                    gfxLine(i * 2, 46 + 9 * ch, i * 2, 46 + 9 * ch + 8);
                }
            }
            /*
            float w = float(64 * 1 + spacing) / length[ch];
            for (int i = 0; i < length[ch]; i++) {
                int x = int(w * i);
                if ((pattern[ch] >> i) & 0x1) {
                    gfxRect(x, 46 + 9 * ch, w - spacing, 6);
                }
                if ((step % length[ch]) == i) {
                    gfxLine(x - 1, 46 + 9 * ch + 7, x + w - 1, 46 + 9 * ch + 7);
                }
            }
            */
            /*
            DrawActiveSegment(ch);
            DrawPatternPoints(ch);
            */
        }
    }

    void DrawActiveSegment(int ch) {
        if (last_clock && OC::CORE::ticks - last_clock < 166666) {
            int s1 = step % length[ch];
            int s2 = s1 + 1 == length[ch] ? 0 : s1 + 1;

            AFStepCoord s1_c = disp_coord[ch][s1];
            AFStepCoord s2_c = disp_coord[ch][s2];
            gfxLine(s1_c.x, s1_c.y, s2_c.x, s2_c.y);
        }
    }

    void DrawPatternPoints(int ch) {
        for (int p = 0; p < length[ch]; p++)
        {
            if ((pattern[ch] >> p) & 0x01) {
                gfxPixel(disp_coord[ch][p].x, disp_coord[ch][p].y);
                gfxPixel(disp_coord[ch][p].x + 1, disp_coord[ch][p].y);
            }
        }
    }

    void DrawEditor() {
        /*
        int spacing = 23;
        int extra_pad = 0;

        int lenx = 1;
        int beatx = lenx + spacing;
        int offx = beatx + spacing;

        ForEachChannel (ch) {
            int y = 15 + 10 * ch;

            // Length cursor
            gfxBitmap(lenx, y, 8, LOOP_ICON);
            gfxPrint(lenx + extra_pad + 8 + pad(10, length[ch]), y, length[ch]);
            if (cursor == 0 + ch * NUM_PARAMS) gfxCursor(lenx + extra_pad + 9, y + 7, 12);

            // Beats cursor
            gfxBitmap(beatx, y, 8, X_NOTE_ICON);
            gfxPrint(beatx + extra_pad + 8 + pad(10, beats[ch]), y, beats[ch]);
            if (cursor == 1 + ch * NUM_PARAMS) gfxCursor(beatx + extra_pad + 9, y + 7, 12);

            // Offset cursor
            gfxBitmap(offx, y, 8, LEFT_RIGHT_ICON);
            gfxPrint(offx + extra_pad + 8 + pad(10, offset[ch]), y, offset[ch]);
            if (cursor == 2 + ch * NUM_PARAMS) gfxCursor(offx + extra_pad + 9, y + 7, 12);
        }
        */

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

        // Ring indicator
        //gfxCircle(8, 52, 8);
        //gfxCircle(8, 52, 4);

        //if (ch == 0) gfxCircle(8, 52, 7);
        //else gfxCircle(8, 52, 5);
    }

    /* Get coordinates of circle in two halves, from the top and from the bottom */
    void SetDisplayPositions(int ch) {
        int r = ch == 0 ? OUTER_RADIUS : INNER_RADIUS;
        int cx = 31; // Center coordinates
        int cy = 39;
        int di = 0; // Display index (positions actually used in the display)
        const float pi = 3.14159265358979323846f;
        float step_radians = 2.0f * pi / length[ch];

        for (int i = 0; i < length[ch]; i++) {
            float rads = i * step_radians - pi / 2.0f;
            uint8_t x = uint8_t(r * cos(rads) + cx);
            uint8_t y = uint8_t(r * sin(rads) + cy);
            disp_coord[ch][di++] = AFStepCoord {x, y};
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
