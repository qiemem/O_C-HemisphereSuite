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
    static constexpr int MAX_TRANS = 32;
    static constexpr int GLIDE_FACTOR = 16;

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
      void ToggleAccent(const size_t s_) {
        note[s_] ^= (0x01 << 6);
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

    void Controller()
    {
      if (Clock(1)) { // reset
        seq.Reset();
      }
      if (Clock(0)) { // clock

        if (!write_mode) {
          // CV modulation of pattern and transposition
          pattern_mod = pattern_index;
          Modulate(pattern_mod, 0, 0, 7);
          SetPattern(pattern_mod);

          trans_mod = transpose;
          Modulate(trans_mod, 1, -MAX_TRANS, MAX_TRANS);
        }

        seq.Advance();

        if (seq.muted(seq.step)) {
          GateOut(1, false);
        } else {
          current_note = seq.GetNote();

          if (seq.accent(seq.step)) {
            // - tied note
            //GateOut(1, true);

            // - 50% duty cycle gate
            ClockOut(1, ClockCycleTicks(0)/2);

          } else {
            // regular trigger
            ClockOut(1);
          }
        }
        StartADCLag();
      }

      if (EndOfADCLag() && write_mode) {
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
      int play_note = current_note + 64 + trans_mod;
      CONSTRAIN(play_note, 0, 127);
      // set CV output
      int play_cv = QuantizerLookup(0, play_note);
      if (seq.accent(seq.step)) {
        // glide
        SmoothedOut(0, play_cv, GLIDE_FACTOR);
      } else {
        Out(0, play_cv);
      }

      if (flash_ticker) --flash_ticker;
      if (edit_ticker) --edit_ticker;
    }

    void View() {
      DrawPanel();
    }

    void OnButtonPress() {
      CursorAction(cursor, MAX_CURSOR);
      if (cursor == WRITE_MODE) // toggle
        write_mode = EditMode();

      // double-click toggles accent
      if (cursor >= NOTES) {
        if ( OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME ) {
          seq.ToggleAccent(cursor - NOTES);
        } else {
          click_tick = OC::CORE::ticks;
        }
      }
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
      
      if (cursor < NOTES) flash_ticker = 1000;

      isEditing = false;
    }

    void SetPattern(int &index) {
      CONSTRAIN(index, 0, 7);
      seq.note = (uint8_t*)(OC::user_patterns[index].notes);
    }
    void OnEncoderMove(int direction) {
      if (!EditMode()) {
        MoveCursor(cursor, direction, MAX_CURSOR - (STEP_COUNT - seq.length));
        return;
      }

      edit_ticker = 5000;
      switch (cursor) {
        default: // (cursor >= NOTES)
          seq.SetNote(seq.GetNote(cursor-NOTES) + direction, cursor-NOTES);
          break;
        case PATTERN:
          // TODO: queued pattern changes
          pattern_index += direction;
          SetPattern(pattern_index);
          pattern_mod = pattern_index;
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
          transpose = constrain(transpose + direction, -MAX_TRANS, MAX_TRANS);
          break;
      }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        // TODO: save Global Settings data to store modified sequences
        Pack(data, PackLocation {0, 4}, pattern_index);
        Pack(data, PackLocation {4, 4}, seqmode);
        Pack(data, PackLocation {8, 6}, seq.length);
        Pack(data, PackLocation {14, 6}, transpose + MAX_TRANS);

        Pack(data, PackLocation {20, 8}, (uint8_t)GetScale(0));
        Pack(data, PackLocation {28, 4}, (uint8_t)GetRootNote(0));

        return data;
    }

    void OnDataReceive(uint64_t data) {
      pattern_mod = pattern_index = Unpack(data, PackLocation {0, 4});
      seqmode = (AccentMode)Unpack(data, PackLocation {4, 4});
      seq.length = Unpack(data, PackLocation {8, 6});
      trans_mod = transpose = Unpack(data, PackLocation {14, 6}) - MAX_TRANS;

      const uint8_t scale = Unpack(data, PackLocation {20,8});
      const uint8_t root_note = Unpack(data, PackLocation {28, 4});

      SetPattern(pattern_index);
      QuantizerConfigure(0, scale);
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
    int trans_mod, pattern_mod;
    bool write_mode = 0;
    uint32_t click_tick = 0;

    int flash_ticker = 0;
    int edit_ticker = 0;

    void DrawSequenceNumber() {
        gfxPrint(0, 13, "#");
        gfxPrint(edit_ticker? pattern_index + 1 : pattern_mod + 1);
        if (edit_ticker == 0 && pattern_mod != pattern_index) gfxIcon(11, 10, CV_ICON);
    }
    void DrawPanel() {
      gfxDottedLine(0, 22, 63, 22, 3);

      switch (cursor) {
        case QUANT_SCALE:
        case QUANT_ROOT:
        case TRANSPOSE:
        {
          gfxPrint(1, 13, OC::scale_names_short[GetScale(0)]);
          gfxPrint(31, 13, OC::Strings::note_names_unpadded[GetRootNote(0)]);

          const int t = edit_ticker? transpose : trans_mod;
          gfxPrint(45, 13, t >= 0 ? "+" : "-");
          gfxPrint(abs(t));
          if (edit_ticker == 0 && trans_mod != transpose) gfxIcon(44, 8, CV_ICON);

          if (cursor == TRANSPOSE)
            gfxCursor(45, 21, 19);
          else
            gfxCursor(1 + (cursor-QUANT_SCALE)*30, 21, (1-(cursor-QUANT_SCALE))*12 + 13);
          break;
        }

        default: // cursor >= NOTES
        {
          DrawSequenceNumber();

          // XXX: probably not the most efficient approach...
          int notenum = seq.GetNote(cursor - NOTES);
          notenum = MIDIQuantizer::NoteNumber( QuantizerLookup(0, notenum + 64) );
          gfxPrint(33, 13, midi_note_numbers[notenum]);
          break;
        }

        case PATTERN:
        case LENGTH:
        case WRITE_MODE:
          DrawSequenceNumber();

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

      if (flash_ticker) gfxInvert(0, 22, 63, 41);
    }

};
