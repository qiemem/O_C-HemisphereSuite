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

// A "tick" is one ISR cycle, which happens 16666.667 times per second, or a million
// times per minute. A "tock" is a metronome beat.

#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#define CLOCK_PPQN 4

static constexpr uint16_t CLOCK_TEMPO_MIN = 10;
static constexpr uint16_t CLOCK_TEMPO_MAX = 300;
static constexpr uint32_t CLOCK_TICKS_MIN = 1000000 / CLOCK_TEMPO_MAX;
static constexpr uint32_t CLOCK_TICKS_MAX = 1000000 / CLOCK_TEMPO_MIN;

constexpr int MIDI_OUT_PPQN = 24;
constexpr int CLOCK_MAX_MULTIPLE = 24;
constexpr int CLOCK_MIN_MULTIPLE = -31; // becomes /32

class ClockManager {
    static ClockManager *instance;

    enum ClockOutput {
        LEFT_CLOCK,
        RIGHT_CLOCK,
        MIDI_CLOCK,
        NR_OF_CLOCKS
    };

    uint16_t tempo; // The set tempo, for display somewhere else
    uint32_t ticks_per_beat; // Based on the selected tempo in BPM
    bool running = 0; // Specifies whether the clock is running for interprocess communication
    bool paused = 0; // Specifies whethr the clock is paused
    bool forwarded = 0; // Master clock forwarding is enabled when true

    uint32_t clock_tick = 0; // tick when a physical clock was received on DIGITAL 1
    uint32_t beat_tick = 0; // The tick to count from
    bool tock[NR_OF_CLOCKS] = {0,0,0}; // The current tock value
    int tocks_per_beat[NR_OF_CLOCKS] = {1, 1, MIDI_OUT_PPQN}; // Multiplier
    int clock_ppqn = 4; // external clock multiple
    bool cycle = 0; // Alternates for each tock, for display purposes
    int count[NR_OF_CLOCKS] = {0,0,0}; // Multiple counter, 0 is a special case when first starting the clock

    bool boop[4]; // Manual triggers

    ClockManager() {
        SetTempoBPM(120);
    }

public:
    static ClockManager *get() {
        if (!instance) instance = new ClockManager;
        return instance;
    }

    void SetMultiply(int multiply, bool ch = 0) {
        multiply = constrain(multiply, CLOCK_MIN_MULTIPLE, CLOCK_MAX_MULTIPLE);
        tocks_per_beat[ch] = multiply;
    }

    // adjusts the expected clock multiple for external clock pulses
    void SetClockPPQN(int clkppqn) {
        clock_ppqn = constrain(clkppqn, 0, 24);
    }

    /* Set ticks per tock, based on one million ticks per minute divided by beats per minute.
     * This is approximate, because the arithmetical value is likely to be fractional, and we
     * need to live with a certain amount of imprecision here. So I'm not even rounding up.
     */
    void SetTempoBPM(uint16_t bpm) {
        bpm = constrain(bpm, CLOCK_TEMPO_MIN, CLOCK_TEMPO_MAX);
        ticks_per_beat = 1000000 / bpm;
        tempo = bpm;
    }

    int GetMultiply(bool ch = 0) {return tocks_per_beat[ch];}
    int GetClockPPQN() { return clock_ppqn; }

    /* Gets the current tempo. This can be used between client processes, like two different
     * hemispheres.
     */
    uint16_t GetTempo() {return tempo;}

    // Resync multipliers, optionally skipping the first tock
    void Reset(bool count_skip = 0) {
        beat_tick = OC::CORE::ticks;
        for (int ch = 0; ch < NR_OF_CLOCKS; ch++) {
            if (tocks_per_beat[ch] > 0 || 0 == count_skip) count[ch] = count_skip;
        }
        cycle = 1 - cycle;
    }

    // used to align the internal clock with incoming clock pulses
    void Nudge(int diff) {
        if (diff > 0) diff--;
        if (diff < 0) diff++;
        beat_tick += diff;
    }

    // called on every tick when clock is running, before all Controllers
    void SyncTrig(bool clocked) {
        uint32_t now = OC::CORE::ticks;

        // Reset only when all multipliers have been met
        bool reset = 1;

        // count and calculate Tocks
        for (int ch = 0; ch < NR_OF_CLOCKS; ch++) {
            if (tocks_per_beat[ch] == 0) { // disabled
                tock[ch] = 0; continue;
            }

            if (tocks_per_beat[ch] > 0) { // multiply
                uint32_t next_tock_tick = beat_tick + count[ch]*ticks_per_beat / static_cast<uint32_t>(tocks_per_beat[ch]);
                tock[ch] = now >= next_tock_tick;
                if (tock[ch]) ++count[ch]; // increment multiplier counter

                reset = reset && (count[ch] > tocks_per_beat[ch]); // multiplier has been exceeded
            } else { // division: -1 becomes /2, -2 becomes /3, etc.
                int div = 1 - tocks_per_beat[ch];
                uint32_t next_beat = beat_tick + (count[ch] ? ticks_per_beat : 0);
                bool beat_exceeded = (now > next_beat);
                if (beat_exceeded) {
                    ++count[ch];
                    tock[ch] = (count[ch] % div) == 1;
                }
                else
                    tock[ch] = 0;

                // resync on every beat
                reset = reset && beat_exceeded;
                if (tock[ch]) count[ch] = 1;
            }

        }
        if (reset) Reset(1); // skip the one we're already on

        // handle syncing to physical clocks
        if (clocked && clock_tick && clock_ppqn) {

            uint32_t clock_diff = now - clock_tick;
            if (clock_ppqn * clock_diff > CLOCK_TICKS_MAX) clock_tick = 0; // too slow, reset clock tracking

            // if there is a previous clock tick, update tempo and sync
            if (clock_tick && clock_diff) {
                // update the tempo
                ticks_per_beat = constrain(clock_ppqn * clock_diff, CLOCK_TICKS_MIN, CLOCK_TICKS_MAX); // time since last clock is new tempo
                tempo = 1000000 / ticks_per_beat; // imprecise, for display purposes

                int ticks_per_clock = ticks_per_beat / clock_ppqn; // rounded down

                // time since last beat
                int tick_offset = now - beat_tick;

                // too long ago? time til next beat
                if (tick_offset > ticks_per_clock / 2) tick_offset -= ticks_per_beat;

                // within half a clock pulse of the nearest beat AND significantly large
                if (abs(tick_offset) < ticks_per_clock / 2 && abs(tick_offset) > 4)
                    Nudge(tick_offset); // nudge the beat towards us

            }
        }
        // clock has been physically ticked
        if (clocked) clock_tick = now;

    }

    void Start(bool p = 0) {
        Reset();
        running = 1;
        paused = p;
    }

    void Stop() {
        running = 0;
        paused = 0;
    }

    void Pause() {paused = 1;}

    void ToggleForwarding() {
        forwarded = 1 - forwarded;
    }

    void SetForwarding(bool f) {forwarded = f;}

    bool IsRunning() {return (running && !paused);}

    bool IsPaused() {return paused;}

    bool IsForwarded() {return forwarded;}

    // beep boop
    void Boop(int ch = 0) {
        boop[ch] = true;
    }
    bool Beep(int ch = 0) {
        if (boop[ch]) {
            boop[ch] = false;
            return true;
        }
        return false;
    }

    /* Returns true if the clock should fire on this tick, based on the current tempo and multiplier */
    bool Tock(int ch = 0) {
        return tock[ch];
    }

    // Returns true if MIDI Clock should be sent on this tick
    bool MIDITock() {
        return Tock(MIDI_CLOCK);
    }

    bool EndOfBeat(bool ch = 0) {return count[ch] == 1;}

    bool Cycle(bool ch = 0) {return cycle;}
};

ClockManager *ClockManager::instance = 0;

#endif // CLOCK_MANAGER_H
