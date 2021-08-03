// Copyright (c) 2021, Bryan Head
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

class Chordinator : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Chordinator";
    }

    void Start() {
        scale = 5;
        root_quantizer.Init();
        chord_quantizer.Init();
        set_scale(scale);
    }

    void Controller() {
        ForEachChannel(ch) {
            if (Clock(ch)) {
                continuous[ch] = 1;
            }
        }

        if (continuous[0] || EndOfADCLag(0)) {
            chord_root_raw = In(0);
            int32_t new_root_pitch = root_quantizer.Process(chord_root_raw, root, 0);
            if (new_root_pitch != chord_root_pitch) {
                update_chord_quantizer();
                chord_root_pitch = new_root_pitch;
            }
            Out(0, chord_root_pitch);
        }

        if(continuous[1] || EndOfADCLag(1)) {
            int32_t pitch = chord_quantizer.Process(In(0), root, 0);
            Out(1, pitch);
        }
    }

    void View() {
        gfxHeader(applet_name());

        gfxPrint(0, 15, OC::scale_names_short[scale]);
        if (cursor == 0) gfxCursor(0, 23, 30);
        gfxPrint(36, 15, OC::Strings::note_names_unpadded[root]);
        if (cursor == 1) gfxCursor(36, 23, 12);

        uint16_t mask = chord_mask;
        for (size_t i = 0; i < active_scale.num_notes; i++) {
            if (mask & 1) {
                gfxRect(4 * i, 25, 3, 3);
            } else {
                gfxPixel(4 * i + 1, 26);
            }
            if (cursor - 2 == i) {
                gfxCursor(4 * i, 30, 3);
            }
        }

    }

    void OnButtonPress() {
        cursor++;
        if (cursor >= 2 + active_scale.num_notes) {
            cursor = 0;
        }
    }

    void OnEncoderMove(int direction) {
        switch (cursor) {
            case 0: set_scale(scale + direction); break;
            case 1: root = constrain(root + direction, 0, 11); break;
            default:
                    chord_mask ^= 1 << (cursor - 2);
                    break;
        }
        update_chord_quantizer();
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0, 8}, scale);
        Pack(data, PackLocation {8, 4}, root);
        Pack(data, PackLocation {12, 16}, chord_mask);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        set_scale(Unpack(data, PackLocation {0, 8}));
        root = Unpack(data, PackLocation {8, 4});
        chord_mask = Unpack(data, PackLocation {12, 16});
        update_chord_quantizer();
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "Sample, Lrn/Sync";
        help[HEMISPHERE_HELP_CVS]      = "Weight, Signal";
        help[HEMISPHERE_HELP_OUTS]     = "Sampled out, Thru";
        help[HEMISPHERE_HELP_ENCODER]  = "Scl,root,wght,lrn";
    }

private:

    braids::Quantizer root_quantizer;
    braids::Quantizer chord_quantizer;
    size_t scale; // SEMI
    int16_t root;
    bool continuous[2];
    braids::Scale active_scale;

    size_t cursor;

    // Leftmost is root, second to left is 2, etc. Defaulting here to basic triad.
    uint16_t chord_mask = 0b10101;

    int16_t chord_root_raw = 0;
    int16_t chord_root_pitch = 0;

    void update_chord_quantizer() {
        size_t num_notes = active_scale.num_notes;
        chord_root_pitch = root_quantizer.Process(chord_root_raw, root, 0);
        uint16_t chord_root = root_quantizer.GetLatestNoteNumber() % num_notes;
        uint16_t mask = rotl32(chord_mask, num_notes, chord_root);
        chord_quantizer.Configure(active_scale, mask);
    }

    void set_scale(size_t value) {
        scale = value;
        active_scale = OC::Scales::GetScale(scale);
        root_quantizer.Configure(active_scale);
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to Chordinator,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
Chordinator Chordinator_instance[2];

void Chordinator_Start(bool hemisphere) {
    Chordinator_instance[hemisphere].BaseStart(hemisphere);
}

void Chordinator_Controller(bool hemisphere, bool forwarding) {
    Chordinator_instance[hemisphere].BaseController(forwarding);
}

void Chordinator_View(bool hemisphere) {
    Chordinator_instance[hemisphere].BaseView();
}

void Chordinator_OnButtonPress(bool hemisphere) {
    Chordinator_instance[hemisphere].OnButtonPress();
}

void Chordinator_OnEncoderMove(bool hemisphere, int direction) {
    Chordinator_instance[hemisphere].OnEncoderMove(direction);
}

void Chordinator_ToggleHelpScreen(bool hemisphere) {
    Chordinator_instance[hemisphere].HelpScreen();
}

uint32_t Chordinator_OnDataRequest(bool hemisphere) {
    return Chordinator_instance[hemisphere].OnDataRequest();
}

void Chordinator_OnDataReceive(bool hemisphere, uint32_t data) {
    Chordinator_instance[hemisphere].OnDataReceive(data);
}
