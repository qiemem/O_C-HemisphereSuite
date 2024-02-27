// Copyright (c) 2024, Nicholas J. Michalek
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

class Seq32 : public HemisphereApplet {
public:

    static constexpr int STEP_COUNT = 32;
    static constexpr int MIN_VALUE = 0;
    static constexpr int MAX_VALUE = 63;

    // sets the function of the accent bit
    enum AccentMode {
      VELOCITY,
      TIE_NOTE,
      OCTAVE_UP,
      DOUBLE_OCTAVE_UP,
    };

    enum Seq32Cursor {
      PATTERN,
      LENGTH,
      NOTES,
      MAX_CURSOR = STEP_COUNT + 2
    };

    typedef struct MiniSeq {
      // packed as 6-bit Note Number and two flags
      uint8_t *note = (uint8_t*)(OC::user_patterns[0].notes);
      int length = STEP_COUNT;
      int step = 0;
      bool reset = 1;

      void Randomize() {
          for (int s = 0; s < STEP_COUNT; s++) note[s] = random(0xff);
      }
      void Advance(size_t starting_point) {
          if (reset) {
            reset = false;
            return;
          }
          if (++step >= length) step = 0;
          // If all the steps have been muted, stay where we were
          //if (muted(step) && step != starting_point) Advance(starting_point);
      }
      int GetNote(size_t s_) {
        // lower 6 bits is note value
        // bipolar -32 to +31
        return int(note[s_] & 0x3f) - 32;
      }
      int GetNote() {
        return GetNote(step);
      }
      void SetNote(int nval, size_t s_) {
        // keep upper 2 bits
        note[s_] &= 0xC0;
        note[s_] |= uint8_t(nval + 32) & 0x3f;
      }
      void SetNote(int nval) {
        SetNote(nval, step);
      }
      bool accent(size_t s_) {
        // second highest bit is accent
        return (note[s_] & (0x01 << 6));
      }
      bool muted(size_t s_) {
        // highest bit is mute
        return (note[s_] & (0x01 << 7));
      }
      void ToggleMute(size_t s_) {
        note[s_] ^= (0x01 << 7);
      }
      void Reset() {
          step = 0;
          reset = true;
      }
    } MiniSeq;

    MiniSeq seq;

    const char* applet_name() { // Maximum 10 characters
        return "Seq32";
    }

    void Start() {
    }

    void Controller() {
        if (In(1) > (24 << 7) ) // 24 semitones == 2V
        {
            // new random sequence if CV2 goes high
            if (!cv2_gate) {
                cv2_gate = 1;
                seq.Randomize();
            }
        }
        else cv2_gate = 0;

        if (Clock(1)) { // reset
          seq.Reset();
        }
        if (Clock(0)) { // clock
          seq.Advance(seq.step);
          // send trigger on first step
          if (!seq.muted(seq.step)) {
            ClockOut(1);
            current_note = seq.GetNote();
          }
        }
        
        // continuously compute CV with transpose
        int transpose = DetentedIn(0) / 128; // 128 ADC steps per semitone
        int play_note = current_note + 60 + transpose;
        play_note = constrain(play_note, 0, 127);
        // set CV output
        Out(0, MIDIQuantizer::CV(play_note));
    }

    void View() {
      DrawPanel();
    }

    void OnButtonPress() {
      CursorAction(cursor, MAX_CURSOR);
    }
    void AuxButton() {
      seq.ToggleMute(cursor - NOTES);
      isEditing = false;
    }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, MAX_CURSOR);
        return;
      }

      if (cursor >= NOTES) {
        seq.SetNote(seq.GetNote(cursor-NOTES) + direction, cursor-NOTES);
      } else if (cursor == PATTERN) {
        pattern_index = constrain(pattern_index + direction, 0, 7);
        seq.note = (uint8_t*)(OC::user_patterns[pattern_index].notes);
        seq.Reset();
      } else if (cursor == LENGTH) {
        seq.length = constrain(seq.length + direction, 1, STEP_COUNT);
      }
      //muted &= ~(0x01 << cursor); // unmute
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // TODO: save Global Settings data to store modified sequences
        Pack(data, PackLocation {0, 4}, pattern_index);
        Pack(data, PackLocation {4, 4}, seqmode);
        return data;
    }

    void OnDataReceive(uint64_t data) {
      /*
      const int b = 6; // bits per step
      for (size_t s = 0; s < STEP_COUNT; s++)
      {
          note[s] = Unpack(data, PackLocation {s * b, b}) + MIN_VALUE;
      }
      muted = Unpack(data, PackLocation {STEP_COUNT * b, STEP_COUNT});
      */
      pattern_index = Unpack(data, PackLocation {0, 4});
      seq.Reset();
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1=Trans 2=RndSeq";
        help[HEMISPHERE_HELP_OUTS]     = "A=CV    B=Trig";
        help[HEMISPHERE_HELP_ENCODER]  = "Edit Step / Mutes";
        //                               "------------------" <-- Size Guide
    }
    
private:
    int cursor = 0;
    int pattern_index;
    AccentMode seqmode;
    int current_note = 0;
    bool cv2_gate = 0;

    void DrawPanel() {
      // dotted horizontal centerline
      //gfxDottedLine(0, 40, 63, 40, 3);

      gfxPrint(0, 13, pattern_index);
      if (cursor >= NOTES) {
        gfxPrint(32, 13, seq.GetNote(cursor - NOTES));
      }

      // Steps - 3x3 pixel little boxes
      for (int s = 0; s < seq.length; s++)
      {
        const int x = 2 + (s % 8)*8;
        const int y = 24 + (s / 8)*10;
        if (seq.muted(s))
          gfxFrame(x, y, 3, 3);
        else
          gfxRect(x, y, 4, 4);
        // TODO: visualize note value

        if (cursor - NOTES == s) {
          gfxFrame(x-1, y-1, 6, 6);
          if (EditMode()) gfxInvert(x-1, y-1, 6, 6);
        }
      }
    }

};
