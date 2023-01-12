// Copyright (c) 2018, Jason Justian
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

/* original 8-step mod by Logarhythm, adapted by djphazer
 */

#include "HSMIDI.h"

// DON'T GO PAST 8!
#define SEQX_STEPS 8

class SequenceX : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "SequenceX";
    }

    void Start() {
        for (int s = 0; s < SEQX_STEPS; s++) note[s] = random(0, 30);
    }

    void Controller() {
        if (Clock(1)) { // reset
            step = 0;
            reset = true;
        }
        if (Clock(0)) { // clock
            if (!reset) Advance(step);
            reset = false;
            // send trigger on first step
            if (step == 0) ClockOut(1);
        }
        
        // continuously compute the note
        int transpose = DetentedIn(0) / 128; // 128 ADC steps per semitone
        int play_note = note[step] + 60 + transpose;
        play_note = constrain(play_note, 0, 127);
        // set CV output
        Out(0, MIDIQuantizer::CV(play_note));

    }

    void View() {
        gfxHeader(applet_name());
        DrawPanel();
    }

    void OnButtonPress() {
        if (++cursor == SEQX_STEPS) cursor = 0;
    }

    void OnEncoderMove(int direction) {
        if (note[cursor] + direction < 0 && cursor > 0) {
            // If turning past zero, set the mute bit for this step
            muted |= (0x01 << cursor);
        } else {
            note[cursor] = constrain(note[cursor] + direction, 0, 30);
            muted &= ~(0x01 << cursor);
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        for (int s = 0; s < SEQX_STEPS; s++)
        {
            Pack(data, PackLocation {uint8_t(s * 5),5}, note[s]);
        }
        Pack(data, PackLocation{25,5}, muted);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        for (int s = 0; s < SEQX_STEPS; s++)
        {
            note[s] = Unpack(data, PackLocation {uint8_t(s * 5),5});
        }
        muted = Unpack(data, PackLocation {25,5});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=Transpose";
        help[HEMISPHERE_HELP_OUTS]     = "A=CV B=Clk Step 1";
        help[HEMISPHERE_HELP_ENCODER]  = "Note";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor = 0;
    char muted = 0; // Bitfield for muted steps; ((muted >> step) & 1) means muted
    int note[SEQX_STEPS]; // Sequence value (0 - 30)
    int step = 0; // Current sequencer step
    bool reset = true;

    void Advance(int starting_point) {
        if (++step == SEQX_STEPS) step = 0;
        // If all the steps have been muted, stay where we were
        if (step_is_muted(step) && step != starting_point) Advance(starting_point);
    }

    void DrawPanel() {
        // Sliders
        for (int s = 0; s < SEQX_STEPS; s++)
        {
            //int x = 6 + (12 * s);
            int x = 6 + (7 * s); // APD:  narrower to fit more
            
            if (!step_is_muted(s)) {
                gfxLine(x, 25, x, 63);

                // When cursor, there's a heavier bar and a solid slider
                if (s == cursor) {
                    gfxLine(x + 1, 25, x + 1, 63);
                    //gfxRect(x - 4, BottomAlign(note[s]), 9, 3);
                    gfxRect(x - 2, BottomAlign(note[s]), 5, 3);  // APD
                } else 
                {
                  //gfxFrame(x - 4, BottomAlign(note[s]), 9, 3);
                  gfxFrame(x - 2, BottomAlign(note[s]), 5, 3);  // APD
                }
                
                // When on this step, there's an indicator circle
                if (s == step) 
                {
                  gfxCircle(x, 20, 3);  //Original

                  // APD
                  //int play_note = note[step];// + 60 + transpose;

                  //gfxPrint(10, 15, "Scale ");
                  //gfxPrint(cursor < 12 ? 1 : 2);
                  //gfxPrint(x, 20, play_note);
                  
                }
            } else if (s == cursor) {
                gfxLine(x, 25, x, 63);
                gfxLine(x + 1, 25, x + 1, 63);
             }
        }
    }

    bool step_is_muted(int step) {
        return (muted & (0x01 << step));
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to SequenceX,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
SequenceX SequenceX_instance[2];

void SequenceX_Start(bool hemisphere) {
    SequenceX_instance[hemisphere].BaseStart(hemisphere);
}

void SequenceX_Controller(bool hemisphere, bool forwarding) {
    SequenceX_instance[hemisphere].BaseController(forwarding);
}

void SequenceX_View(bool hemisphere) {
    SequenceX_instance[hemisphere].BaseView();
}

void SequenceX_OnButtonPress(bool hemisphere) {
    SequenceX_instance[hemisphere].OnButtonPress();
}

void SequenceX_OnEncoderMove(bool hemisphere, int direction) {
    SequenceX_instance[hemisphere].OnEncoderMove(direction);
}

void SequenceX_ToggleHelpScreen(bool hemisphere) {
    SequenceX_instance[hemisphere].HelpScreen();
}

uint64_t SequenceX_OnDataRequest(bool hemisphere) {
    return SequenceX_instance[hemisphere].OnDataRequest();
}

void SequenceX_OnDataReceive(bool hemisphere, uint64_t data) {
    SequenceX_instance[hemisphere].OnDataReceive(data);
}
