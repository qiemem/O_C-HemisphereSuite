// Copyright (c) 2018, Jason Justian
// Copyright (c) 2022, Nicholas J. Michalek
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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

/*
 * Turing Machine based on https://thonk.co.uk/documents/random%20sequencer%20documentation%20v2-1%20THONK%20KIT_LargeImages.pdf
 *
 * Thanks to Tom Whitwell for creating the concept, and for clarifying some things
 * Thanks to Jon Wheeler for the CV length and probability updates
 *
 * Heavily adapted as DualTM from ShiftReg/TM by djphazer (Nicholas J. Michalek)
 */

#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

#define TM2_MAX_SCALE OC::Scales::NUM_SCALES

#define TM2_MIN_LENGTH 2
#define TM2_MAX_LENGTH 32

class DualTM : public HemisphereApplet {
public:

    const char* applet_name() {
        return "DualTM";
    }

    void Start() {
        reg = random(0, 65535);
        reg2 = ~reg;
        Output[0] = 0;
        Output[1] = 0;
        p = 0;
        length = 16;
        quant_range = 24;
        cursor = 0;
        quantizer.Init();
        scale = OC::Scales::SCALE_SEMI;
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff); // Semi-tone
    }

    void Controller() {
        bool clk = Clock(0);

        int cv_data[2];
        cv_data[0] = DetentedIn(0);
        cv_data[1] = DetentedIn(1);

        // default to no mod
        p_mod = p;
        len_mod = length;
        range_mod = quant_range;
        smooth_mod = smoothing;
        int note_trans1 = 0;
        int note_trans2 = 0;
        int note_trans3 = 0;

        // process CV inputs
        ForEachChannel(ch) {
            // 0=length; 1=p_mod; 2=range; 3=trans1; 4=trans2; 5=trans1+2
            switch (cvmode[ch]) {
            case -1: // bi-polar mod of slew factor
                smooth_mod = constrain(smooth_mod + Proportion(cv_data[ch], HEMISPHERE_MAX_CV, 128), 1, 128);
                break;
            case 0: // bi-polar modulation of length
                len_mod = constrain(len_mod + Proportion(cv_data[ch], HEMISPHERE_MAX_CV, TM2_MAX_LENGTH), TM2_MIN_LENGTH, TM2_MAX_LENGTH);
                break;

            case 1: // bi-polar modulation of probability
                p_mod = constrain(p_mod + Proportion(cv_data[ch], HEMISPHERE_MAX_CV, 100), 0, 100);
                break;

            case 2: // bi-polar modulation of note range
                range_mod = constrain(range_mod + Proportion(cv_data[ch], HEMISPHERE_MAX_CV, 32), 1, 32);
                break;

            case 3: // bi-polar transpose before quantize
                note_trans1 = Proportion(cv_data[ch], HEMISPHERE_MAX_CV, range_mod);
                break;
            case 4:
                note_trans2 = Proportion(cv_data[ch], HEMISPHERE_MAX_CV, range_mod);
                break;
            case 5:
                note_trans3 = Proportion(cv_data[ch], HEMISPHERE_MAX_CV, range_mod);
                break;
            default: break;
            }
        }

        // Advance the register on clock, flipping bits as necessary
        if (clk) {
            // If the cursor is not on the p value, and Digital 2 is not gated, the sequence remains the same
            int prob = (cursor == 1 || Gate(1)) ? p_mod : 0;

            // Grab the bit that's about to be shifted away
            int last = (reg >> (len_mod - 1)) & 0x01;
            int last2 = (reg2 >> (len_mod - 1)) & 0x01;

            // Does it change?
            if (random(0, 99) < prob) last = 1 - last;
            if (random(0, 99) < prob) last2 = 1 - last2;

            // Shift left, then potentially add the bit from the other side
            reg = (reg << 1) + last;
            reg2 = (reg2 << 1) + last2;
        }
 
        // Send 8-bit scaled and quantized CV
        // scaled = note * quant_range / 0x1f
        int32_t note = Proportion(reg & 0xff, 0xff, range_mod);
        int32_t note2 = Proportion(reg2 & 0xff, 0xff, range_mod);

        /*
        note *= quant_range;
        simfloat x = int2simfloat(note) / (int32_t)0x1f;
        note = simfloat2int(x);
        */

        ForEachChannel(ch) {
            switch (outmode[ch]) {
            case -1: // pitch 1+2
              Output[ch] = slew(Output[ch], quantizer.Lookup(note + note2 + note_trans3 + 64));
              break;
            case 0: // pitch 1
              Output[ch] = slew(Output[ch], quantizer.Lookup(note + note_trans1 + note_trans3 + 64));
              break;
            case 1: // pitch 2
              Output[ch] = slew(Output[ch], quantizer.Lookup(note2 + note_trans2 + note_trans3 + 64));
              break;
            case 2: // mod A - 8-bit bi-polar proportioned CV
              Output[ch] = slew(Output[ch], Proportion( int(reg & 0xff)-0x7f, 0x80, HEMISPHERE_MAX_CV) );
              break;
            case 3: // mod B
              Output[ch] = slew(Output[ch], Proportion( int(reg2 & 0xff)-0x7f, 0x80, HEMISPHERE_MAX_CV) );
              break;
            case 4: // trig A
              if (clk && (reg & 0x01) == 1) // trigger if 1st bit is high
                Output[ch] = HEMISPHERE_MAX_CV; //ClockOut(ch);
              else // decay
                Output[ch] = slew(Output[ch]);
              break;
            case 5: // trig B
              if (clk && (reg2 & 0x01) == 1) // trigger if 1st bit is high
                Output[ch] = HEMISPHERE_MAX_CV; //ClockOut(ch);
              else
                Output[ch] = slew(Output[ch]);
              break;
            case 6: // gate A
              Output[ch] = slew(Output[ch], (reg & 0x01)*HEMISPHERE_MAX_CV );
              break;
            case 7: // gate B
              Output[ch] = slew(Output[ch], (reg2 & 0x01)*HEMISPHERE_MAX_CV );
              break;
            case 8: // gate A+B
              Output[ch] = slew(Output[ch], ((reg & 0x01)+(reg2 & 0x01))*HEMISPHERE_3V_CV );
              break;
            }

            Out(ch, Output[ch]);
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawSelector();
        DrawIndicator();
    }

    void OnButtonPress() {
        isEditing = !isEditing;
    }

    void OnEncoderMove(int direction) {
        if (!isEditing) {
            cursor = constrain(cursor + direction, 0, 8);
            ResetCursor();  // Reset blink so it's immediately visible when moved
        } else {
            switch (cursor) {
            case 0:
                length = constrain(length + direction, TM2_MIN_LENGTH, TM2_MAX_LENGTH);
                break;
            case 1:
                p = constrain(p + direction, 0, 100);
                break;
            case 2:
                scale += direction;
                if (scale >= TM2_MAX_SCALE) scale = 0;
                if (scale < 0) scale = TM2_MAX_SCALE - 1;
                quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);
                break;
            case 3:
                quant_range = constrain(quant_range + direction, 1, 32);
                break;
            case 4:
                outmode[0] = constrain(outmode[0] + direction, -1, 8);
                break;
            case 5:
                outmode[1] = constrain(outmode[1] + direction, -1, 8);
                break;
            case 6:
                cvmode[0] = constrain(cvmode[0] + direction, -1, 5);
                break;
            case 7:
                cvmode[1] = constrain(cvmode[1] + direction, -1, 5);
                break;
            case 8:
                smoothing = constrain(smoothing + direction, 1, 128);
                break;
            }
        }
    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,7}, p);
        Pack(data, PackLocation {7,5}, length - 1);
        Pack(data, PackLocation {12,5}, quant_range - 1);
        Pack(data, PackLocation {17,4}, outmode[0]+1);
        Pack(data, PackLocation {21,4}, outmode[1]+1);
        Pack(data, PackLocation {25,8}, constrain(scale, 0, 255));
        Pack(data, PackLocation {33,4}, cvmode[0]+1);
        Pack(data, PackLocation {37,4}, cvmode[1]+1);

        // maybe don't bother saving the damn register
        //Pack(data, PackLocation {32,32}, reg);

        return data;
    }

    void OnDataReceive(uint64_t data) {
        p = Unpack(data, PackLocation {0,7});
        length = Unpack(data, PackLocation {7,5}) + 1;
        quant_range = Unpack(data, PackLocation{12,5}) + 1;
        outmode[0] = Unpack(data, PackLocation {17,4}) - 1;
        outmode[1] = Unpack(data, PackLocation {21,4}) - 1;
        scale = Unpack(data, PackLocation {25,8});
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);
        cvmode[0] = Unpack(data, PackLocation {33,4}) - 1;
        cvmode[1] = Unpack(data, PackLocation {37,4}) - 1;

        reg = Unpack(data, PackLocation {32,32});
        reg2 = Unpack(data, PackLocation {0, 32}); // lol it could be fun
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=p Gate";
        help[HEMISPHERE_HELP_CVS]      = "Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "Assignable";
        help[HEMISPHERE_HELP_ENCODER]  = "Select/Push 2 Edit";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int length; // Sequence length
    int len_mod; // actual length after CV mod
    int cursor;  // 0 = length, 1 = p, 2 = scale, 3=range, 4=OutA, 5=OutB, 6=CV1, 7=CV2
    bool isEditing = false;
    braids::Quantizer quantizer;

    // Settings
    uint32_t reg; // 32-bit sequence register
    uint32_t reg2; // DJP

    int Output[2];

    int p; // Probability of bit flipping on each cycle
    int p_mod;
    int scale; // Scale used for quantized output
    int quant_range;
    int range_mod;
    int smoothing = 4;
    int smooth_mod;

    // output modes:
    // 0=pitch A; 2=mod A; 4=trig A; 6=gate A
    // 1=pitch B; 3=mod B; 5=trig B; 7=gate B
    // -1=pitch A+B; 8=gate A+B
    int outmode[2] = {0, 5};

    // CV input mappings:
    // 0=length; 1=p_mod; 2=range; 3=trans1; 4=trans2; 5=trans1+2
    int cvmode[2] = {0, 5};

    int slew(int old_val, const int new_val = 0) {
        // more smoothing causes more ticks to be skipped
        if (OC::CORE::ticks % smooth_mod) return old_val;

        old_val = (old_val * (smooth_mod - 1) + new_val) / smooth_mod;
        return old_val; 
    }

    void DrawSelector() {
        gfxBitmap(1, 14, 8, LOOP_ICON);
        gfxPrint(12 + pad(10, len_mod), 15, len_mod);
        gfxPrint(32, 15, "p=");
        if (cursor == 1 || Gate(1)) { // p unlocked
            gfxPrint(pad(100, p_mod), p_mod);
        } else { // p is disabled
            gfxBitmap(49, 14, 8, LOCK_ICON);
        }
        gfxBitmap(1, 25, 8, SCALE_ICON);
        gfxPrint(12, 25, OC::scale_names_short[scale]);
        gfxBitmap(41, 25, 8, UP_DOWN_ICON);
        gfxPrint(49, 25, range_mod); // APD

        if (cursor < 6) {
            gfxPrint(1, 36, "A:");
            gfxPrint(32, 36, "B:");

            ForEachChannel(ch) {
                switch (outmode[ch]) {
                case -1: gfxBitmap(24+ch*32, 35, 3, SUP_ONE);
                case 0: // pitch output
                case 1:
                    gfxBitmap(15 + ch*32, 35, 8, NOTE_ICON);
                    break;
                case 2: // mod output
                case 3:
                    gfxBitmap(15 + ch*32, 35, 8, WAVEFORM_ICON);
                    break;
                case 4: // trig output
                case 5:
                    gfxBitmap(15 + ch*32, 35, 8, CLOCK_ICON);
                    break;
                case 8: gfxBitmap(24+ch*32, 35, 3, SUB_TWO);
                case 6: // gate output
                case 7:
                    gfxBitmap(15 + ch*32, 35, 8, METER_ICON);
                    break;
                }
                // indicator for reg1 or reg2
                gfxBitmap(24+ch*32, 35, 3, (outmode[ch] % 2) ? SUB_TWO : SUP_ONE);
            }
        } else if (cursor < 8) { // CV inputs
            gfxIcon(1, 35, CV_ICON);
            gfxBitmap(9, 35, 3, SUP_ONE);
            gfxIcon(32, 35, CV_ICON);
            gfxBitmap(40, 35, 3, SUB_TWO);

            ForEachChannel(ch) {
                // 0=length; 1=p_mod; 2=range; 3=trans1; 4=trans2; 5=trans1+2
                switch (cvmode[ch]) {
                case -1: // smoothing
                    gfxIcon(15 + ch*32, 35, MOD_ICON);
                    break;
                case 0:
                    gfxIcon(15 + ch*32, 35, LOOP_ICON);
                    break;
                case 1:
                    gfxPrint(15 + ch*32, 35, "p");
                    break;
                case 2:
                    gfxIcon(15 + ch*32, 35, UP_DOWN_ICON);
                    break;
                case 3:
                    gfxIcon(15 + ch*32, 35, BEND_ICON);
                    gfxBitmap(24+ch*32, 35, 3, SUP_ONE);
                    break;
                case 5:
                    gfxBitmap(24+ch*32, 35, 3, SUP_ONE);
                case 4:
                    gfxIcon(15 + ch*32, 35, BEND_ICON);
                    gfxBitmap(24+ch*32, 35, 3, SUB_TWO);
                    break;
                }
            }
        } else { // smoothing
            gfxPrint(1, 35, "Slew:");
            gfxPrint(smooth_mod);

            gfxCursor(31, 43, 18);
            if (isEditing) gfxInvert(31, 34, 18, 9);
        }

        switch (cursor) {
            case 0: gfxCursor(13, 23, 12); break; // Length Cursor
            case 1: gfxCursor(45, 23, 18); break; // Probability Cursor
            case 2: gfxCursor(12, 33, 25); break; // Scale Cursor
            case 3: gfxCursor(49, 33, 14); break; // Quant Range Cursor // APD
            case 6:
            case 4: gfxCursor(14, 43, 10); break; // Out A / CV 1
            case 7:
            case 5: gfxCursor(46, 43, 10); break; // Out B / CV 2
        }
        if (isEditing) {
            switch (cursor) {
                case 0: gfxInvert(13, 14, 12, 9); break;
                case 1: gfxInvert(45, 14, 18, 9); break;
                case 2: gfxInvert(12, 24, 25, 9); break;
                case 3: gfxInvert(49, 24, 14, 9); break;
                case 6:
                case 4: gfxInvert(14, 34, 14, 9); break;
                case 7:
                case 5: gfxInvert(46, 34, 14, 9); break;
            }
        }
    }

    void DrawIndicator() {
        gfxLine(0, 45, 63, 45);
        gfxLine(0, 62, 63, 62);
        for (int b = 0; b < 16; b++)
        {
            int v = (reg >> b) & 0x01;
            int v2 = (reg2 >> b) & 0x01;
            if (v) gfxRect(60 - (4 * b), 47, 3, 7);
            if (v2) gfxRect(60 - (4 * b), 54, 3, 7);
        }
    }

};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to DualTM,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
DualTM DualTM_instance[2];

void DualTM_Start(bool hemisphere) {
    DualTM_instance[hemisphere].BaseStart(hemisphere);
}

void DualTM_Controller(bool hemisphere, bool forwarding) {
    DualTM_instance[hemisphere].BaseController(forwarding);
}

void DualTM_View(bool hemisphere) {
    DualTM_instance[hemisphere].BaseView();
}

void DualTM_OnButtonPress(bool hemisphere) {
    DualTM_instance[hemisphere].OnButtonPress();
}

void DualTM_OnEncoderMove(bool hemisphere, int direction) {
    DualTM_instance[hemisphere].OnEncoderMove(direction);
}

void DualTM_ToggleHelpScreen(bool hemisphere) {
    DualTM_instance[hemisphere].HelpScreen();
}

uint64_t DualTM_OnDataRequest(bool hemisphere) {
    return DualTM_instance[hemisphere].OnDataRequest();
}

void DualTM_OnDataReceive(bool hemisphere, uint64_t data) {
    DualTM_instance[hemisphere].OnDataReceive(data);
}
