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

#define MIDI_CLOCK 0xF8
#define MIDI_START 0xFA
#define MIDI_STOP  0xFC

class ClockSetup : public HemisphereApplet {
public:

    const char* applet_name() {
        return "ClockSet";
    }

    void Start() { }

    // The ClockSetup controller handles MIDI Clock and Transport Start/Stop
    void Controller() {
        if (start_q){
            start_q = 0;
            usbMIDI.sendRealTime(MIDI_START);
        }
        if (stop_q){
            stop_q = 0;
            usbMIDI.sendRealTime(MIDI_STOP);
        }
        if (clock_m->IsRunning() && clock_m->MIDITock()) usbMIDI.sendRealTime(MIDI_CLOCK);
    }

    void View() {
        DrawInterface();
    }

    void OnButtonPress() {
        if (cursor == 0) PlayStop();
        else if (cursor == 1) clock_m->ToggleForwarding();
        else if (cursor > 5) clock_m->Boop(cursor-6);
        else isEditing = !isEditing;
    }

    void OnEncoderMove(int direction) {
        if (!isEditing) {
            cursor = constrain(cursor + direction, 0, 9);
            ResetCursor();
        } else {
            switch (cursor) {
            case 2: // input PPQN
                clock_m->SetClockPPQN(clock_m->GetClockPPQN() + direction);
                break;
            case 3: // Set tempo
                clock_m->SetTempoBPM(clock_m->GetTempo() + direction);
                break;

            case 4: // Set multiplier
            case 5:
                clock_m->SetMultiply(clock_m->GetMultiply(cursor-4) + direction, cursor-4);
                break;
            }
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation { 0, 1 }, clock_m->IsRunning() || clock_m->IsPaused());
        Pack(data, PackLocation { 1, 9 }, clock_m->GetTempo());
        Pack(data, PackLocation { 10, 5 }, clock_m->GetMultiply());
        Pack(data, PackLocation { 15, 1 }, clock_m->IsForwarded());
        return data;
    }

    void OnDataReceive(uint64_t data) {
        if (Unpack(data, PackLocation { 0, 1 })) {
            clock_m->Start(1); // start paused
        } else {
            clock_m->Stop();
        }
        clock_m->SetTempoBPM(Unpack(data, PackLocation { 1, 9 }));
        clock_m->SetMultiply(Unpack(data, PackLocation { 10, 5 }),0);
        clock_m->SetMultiply(Unpack(data, PackLocation { 10, 5 }),1);
        clock_m->SetForwarding(Unpack(data, PackLocation { 15, 1 }));
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "";
        help[HEMISPHERE_HELP_CVS]      = "";
        help[HEMISPHERE_HELP_OUTS]     = "";
        help[HEMISPHERE_HELP_ENCODER]  = "";
        //                               "------------------" <-- Size Guide
    }

private:
    char cursor; // 0=Play/Stop, 1=Forwarding, 2=External PPQN, 3=Tempo, 4,5=Multiply
    bool isEditing = false;
    bool start_q;
    bool stop_q;
    ClockManager *clock_m = clock_m->get();

    void PlayStop() {
        if (clock_m->IsRunning()) {
            stop_q = 1;
            clock_m->Stop();
        } else {
            start_q = 1;
            clock_m->Start();
        }
    }

    void DrawInterface() {
        // Header: This is sort of a faux applet, so its header
        // needs to extend across the screen
        graphics.setPrintPos(1, 2);
        graphics.print("Clock Setup");
        //gfxLine(0, 10, 62, 10);
        //gfxLine(0, 12, 62, 12);
        graphics.drawLine(0, 10, 127, 10);
        graphics.drawLine(0, 12, 127, 12);

        // Clock Source
        gfxIcon(1, 15, CLOCK_ICON);
        if (clock_m->IsRunning()) {
            gfxIcon(12, 15, PLAY_ICON);
        } else if (clock_m->IsPaused()) {
            gfxIcon(12, 15, PAUSE_ICON);
        } else {
            gfxIcon(12, 15, STOP_ICON);
        }
        gfxPrint(26, 15, "Fwd ");
        gfxIcon(50, 15, clock_m->IsForwarded() ? CHECK_ON_ICON : CHECK_OFF_ICON);

        // Input PPQN
        gfxPrint(64, 15, "PPQN x");
        gfxPrint(clock_m->GetClockPPQN());

        // Tempo
        gfxIcon(1, 26, NOTE4_ICON);
        gfxPrint(9, 26, "= ");
        gfxPrint(pad(100, clock_m->GetTempo()), clock_m->GetTempo());
        gfxPrint(" BPM");

        // Multiply
        gfxPrint(1, 37, "x");
        gfxPrint(clock_m->GetMultiply(0));

        // secondary multiplier when forwarding internal clock
        gfxPrint(65, 37, "x");
        gfxPrint(clock_m->GetMultiply(1));

        // Manual triggers
        gfxIcon(4, 49, BURST_ICON);
        gfxIcon(36, 49, BURST_ICON);
        gfxIcon(68, 49, BURST_ICON);
        gfxIcon(100, 49, BURST_ICON);

        switch (cursor) {
        case 0: gfxFrame(11, 14, 10, 10); break; // Play/Stop
        case 1: gfxFrame(49, 14, 10, 10); break; // Forwarding

        case 2: // Clock PPQN
            isEditing ? gfxInvert(100,14, 12,9) : gfxCursor(100,23, 12);
            break;

        case 3: // BPM
            isEditing ? gfxInvert(22, 25, 18, 9) : gfxCursor(22, 34, 18);
            break;

        case 4: // Multipliers
        case 5:
            isEditing ? gfxInvert(8 + 64*(cursor-4), 36, 12, 9) : gfxCursor(8 + 64*(cursor-4), 45, 12);
            break;


        case 6: gfxFrame(3, 48, 10, 10); break; // Manual Trig 1
        case 7: gfxFrame(35, 48, 10, 10); break; // Manual Trig 2
        case 8: gfxFrame(67, 48, 10, 10); break; // Manual Trig 3
        case 9: gfxFrame(99, 48, 10, 10); break; // Manual Trig 4
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to ClockSetup,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
ClockSetup ClockSetup_instance[1];

void ClockSetup_Start(bool hemisphere) {ClockSetup_instance[hemisphere].BaseStart(hemisphere);}
void ClockSetup_Controller(bool hemisphere, bool forwarding) {ClockSetup_instance[hemisphere].BaseController(forwarding);}
void ClockSetup_View(bool hemisphere) {ClockSetup_instance[hemisphere].BaseView();}
void ClockSetup_OnButtonPress(bool hemisphere) {ClockSetup_instance[hemisphere].OnButtonPress();}
void ClockSetup_OnEncoderMove(bool hemisphere, int direction) {ClockSetup_instance[hemisphere].OnEncoderMove(direction);}
void ClockSetup_ToggleHelpScreen(bool hemisphere) {ClockSetup_instance[hemisphere].HelpScreen();}
uint64_t ClockSetup_OnDataRequest(bool hemisphere) {return ClockSetup_instance[hemisphere].OnDataRequest();}
void ClockSetup_OnDataReceive(bool hemisphere, uint64_t data) {ClockSetup_instance[hemisphere].OnDataReceive(data);}
