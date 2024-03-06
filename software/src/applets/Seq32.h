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
      // ---
      PATTERN,
      LENGTH,
      WRITE_MODE,
      // ---
      QUANT_SCALE,
      QUANT_ROOT,
      TRANSPOSE,
      // ---
      NOTES,
      MAX_CURSOR = TRANSPOSE + STEP_COUNT
    };

    typedef struct MiniSeq {
      // packed as 6-bit Note Number and two flags
      uint8_t *note = (uint8_t*)(OC::user_patterns[0].notes);
      int length = STEP_COUNT;
      int step = 0;
      bool reset = 1;

      void Clear() {
          for (int s = 0; s < STEP_COUNT; s++) note[s] = 0x20; // C4 == 0V
      }
      void Randomize() {
        for (int s = 0; s < STEP_COUNT; s++) {
          note[s] = random(0xff);
        }
      }
      void SowPitches(const uint8_t range = 32) {
        for (int s = 0; s < STEP_COUNT; s++) {
          SetNote(random(range), s);
        }
      }
      void Advance() {
          if (reset) {
            reset = false;
            return;
          }
          if (++step >= length) step = 0;
      }
      int GetNote(const size_t s_) {
        // lower 6 bits is note value
        // bipolar -32 to +31
        return int(note[s_] & 0x3f) - 32;
      }
      int GetNote() {
        return GetNote(step);
      }
      void SetNote(int nval, const size_t s_) {
        nval += 32;
        CONSTRAIN(nval, MIN_VALUE, MAX_VALUE);
        // keep upper 2 bits
        note[s_] = (note[s_] & 0xC0) | (uint8_t(nval) & 0x3f);
        //note[s_] &= ~(0x01 << 7); // unmute?
      }
      void SetNote(int nval) {
        SetNote(nval, step);
      }
      bool accent(const size_t s_) {
        // second highest bit is accent
        return (note[s_] & (0x01 << 6));
      }
      void SetAccent(const size_t s_, bool on = true) {
        note[s_] &= ~(1 << 6); // clear
        note[s_] |= (on << 6); // set
      }
      bool muted(const size_t s_) {
        // highest bit is mute
        return (note[s_] & (0x01 << 7));
      }
      void Unmute(const size_t s_) {
        note[s_] &= ~(0x01 << 7);
      }
      void Mute(const size_t s_, bool on = true) {
        note[s_] |= (on << 7);
      }
      void ToggleMute(const size_t s_) {
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
      /* this isn't really what I want cv2 to do...
        if (In(1) > (24 << 7) ) // 24 semitones == 2V
        {
            // new random sequence if CV2 goes high
            if (!cv2_gate) {
                cv2_gate = 1;
                seq.Randomize();
            }
        }
        else cv2_gate = 0;
      */

        if (Clock(1)) { // reset
          seq.Reset();
        }
        if (Clock(0)) { // clock
          seq.Advance();
          // send trigger on first step
          if (!seq.muted(seq.step)) {
            ClockOut(1);
            current_note = seq.GetNote();
          }
          StartADCLag();
        }

        // set flag from UI, or hold cv2 high to record
        if (EndOfADCLag() && (write_mode) ) {
          // sample and record note number from cv1
          Quantize(0, In(0));
          current_note = GetLatestNoteNumber(0) - 64;
          seq.SetNote(current_note, seq.step);

          if (In(1) > (6 << 7)) // cv2 > 0.5V determines mute state
            seq.Unmute(seq.step);
          else
            seq.Mute(seq.step);

          seq.SetAccent(seq.step, In(1) > (24 << 7)); // cv2 > 2V qualifies as accent
        }
        
        // continuously compute CV with transpose
        // I don't always want transpose here... it should be assignable.
        //int transpose = MIDIQuantizer::NoteNumber(DetentedIn(0)) - 60; // 128 ADC steps per semitone

        int play_note = current_note + 64 + transpose;
        play_note = constrain(play_note, 0, 127);
        // set CV output
        Out(0, QuantizerLookup(0, play_note));
    }

    void View() {
      DrawPanel();
    }

    void OnButtonPress() {
      if (cursor == WRITE_MODE) // toggle
        write_mode = !write_mode;
      else
        CursorAction(cursor, MAX_CURSOR);
    }
    void AuxButton() {
      if (cursor >= NOTES)
        seq.ToggleMute(cursor - NOTES);
      else if (cursor == LENGTH)
        seq.Randomize();
      else if (cursor == TRANSPOSE)
        seq.SowPitches(abs(transpose));
      else if (cursor == PATTERN)
        seq.Clear();

      isEditing = false;
    }

    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, MAX_CURSOR - (STEP_COUNT - seq.length));
        return;
      }

      switch (cursor) {
        default: // (cursor >= NOTES)
          seq.SetNote(seq.GetNote(cursor-NOTES) + direction, cursor-NOTES);
          break;
        case PATTERN:
          // TODO: queued pattern changes
          pattern_index = constrain(pattern_index + direction, 0, 7);
          seq.note = (uint8_t*)(OC::user_patterns[pattern_index].notes);
          //seq.Reset();
          break;

        case LENGTH:
          seq.length = constrain(seq.length + direction, 1, STEP_COUNT);
          break;

        case QUANT_SCALE:
          NudgeScale(0, direction);
          break;

        case QUANT_ROOT:
        {
          int root_note = GetRootNote(0);
          SetRootNote(0, constrain(root_note + direction, 0, 11));
          break;
        }

        case TRANSPOSE:
          transpose = constrain(transpose + direction, -36, 36);
          break;
      }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // TODO: save Global Settings data to store modified sequences
        Pack(data, PackLocation {0, 4}, pattern_index);
        Pack(data, PackLocation {4, 4}, seqmode);
        Pack(data, PackLocation {8, 6}, seq.length);
        Pack(data, PackLocation {14, 6}, transpose);

        Pack(data, PackLocation {20, 8}, constrain(GetScale(0), 0, 255));
        Pack(data, PackLocation {28, 4}, GetRootNote(0));

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
      seqmode = (AccentMode)Unpack(data, PackLocation {4, 4});
      seq.length = Unpack(data, PackLocation {8, 6});
      transpose = Unpack(data, PackLocation {14, 6});

      const int scale = Unpack(data, PackLocation {20,8});
      QuantizerConfigure(0, scale);

      const int root_note = Unpack(data, PackLocation {28, 4});
      SetRootNote(0, root_note);

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
    int transpose = 0;
    uint8_t range_ = 32;
    bool write_mode = 0;
    bool cv2_gate = 0;

    void DrawPanel() {
      gfxDottedLine(0, 22, 63, 22, 3);

      switch (cursor) {
        case QUANT_SCALE:
        case QUANT_ROOT:
        case TRANSPOSE:
          gfxPrint(1, 13, OC::scale_names_short[GetScale(0)]);
          gfxPrint(31, 13, OC::Strings::note_names_unpadded[GetRootNote(0)]);
          gfxPrint(45, 13, transpose >= 0 ? "+" : "-");
          gfxPrint(abs(transpose));

          if (cursor == TRANSPOSE)
            gfxCursor(45, 21, 19);
          else
            gfxCursor(1 + (cursor-QUANT_SCALE)*30, 21, (1-(cursor-QUANT_SCALE))*12 + 13);
          break;

        default: // cursor >= NOTES
        {
          gfxPrint(0, 13, "#");
          gfxPrint(pattern_index + 1);

          // XXX: probably not the most efficient approach...
          int notenum = seq.GetNote(cursor - NOTES);
          notenum = MIDIQuantizer::NoteNumber( QuantizerLookup(0, notenum + 64) );
          gfxPrint(33, 13, midi_note_numbers[notenum]);
          break;
        }

        case PATTERN:
        case LENGTH:
        case WRITE_MODE:
          gfxPrint(0, 13, "#");
          gfxPrint(pattern_index + 1);

          gfxIcon(24, 13, LOOP_ICON);
          gfxPrint(33, 13, seq.length);
          gfxIcon(49, 13, RECORD_ICON);
          if (cursor == WRITE_MODE)
            gfxFrame(48, 12, 10, 10);
          else
            gfxCursor(6 + (cursor-PATTERN)*27, 21, 13);

          if (write_mode)
            gfxInvert(48, 12, 10, 10);
          break;
      }

      // Draw steps
      for (int s = 0; s < seq.length; s++)
      {
        const int x = 2 + (s % 8)*8;
        const int y = 26 + (s / 8)*10;
        if (!seq.muted(s))
        {
          if (seq.accent(s))
            gfxRect(x-1, y-1, 5, 5);
          else
            gfxFrame(x-1, y-1, 5, 5);
        }

        if (seq.step == s)
          gfxIcon(x-2, y-7, DOWN_BTN_ICON);
        // TODO: visualize note value

        if (cursor - NOTES == s) {
          gfxFrame(x-2, y-2, 8, 10);
          if (EditMode()) gfxInvert(x-1, y-1, 6, 8);
        }
      }
    }

};
