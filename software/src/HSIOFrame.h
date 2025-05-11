/* Copyright (c) 2023-2024 Nicholas J. Michalek & Beau Sterling
 *
 * IOFrame & friends
 *   attempts at making applet I/O more flexible and portable
 *
 * Some processing logic adapted from the MIDI In applet
 *
 */

#pragma once

#include <vector>
#include "HSMIDI.h"
#include "HSUtils.h"
#include "OC_DAC.h"

namespace HS {

static constexpr int GATE_THRESHOLD = 15 << 7; // 1.25 volts
static constexpr int TRIGMAP_MAX = OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST + DAC_CHANNEL_LAST;
static constexpr int CVMAP_MAX = ADC_CHANNEL_LAST + DAC_CHANNEL_LAST;
static constexpr int MIDIMAP_MAX = 32;

struct MIDILogEntry {
    uint8_t message;
    uint8_t data1;
    uint8_t data2;
};

struct MIDINoteData {
    uint8_t note; // data1
    uint8_t vel;  // data2
};

struct PolyphonyData {
    uint8_t note;
    uint8_t vel;
    bool gate;
};

struct MIDIMapping {
  int function;
  int16_t function_cc;
  uint8_t channel; // MIDI channel number
  uint8_t dac_polyvoice; // select which voice to send from output
  uint16_t semitone_mask; // which notes are currently on
  bool trigout_q; // TRIGGA
  int output; // translated CV values
};

using NoteBuffer = std::vector<MIDINoteData>;

struct MIDIFrame {
    MIDIMapping mapping[MIDIMAP_MAX];

    // MIDI input stuff handled by MIDIIn applet
    NoteBuffer note_buffer[16]; // note buffer to track all held notes on all channels
    uint8_t last_midi_channel = 0; // for MIDI In activity monitor
    uint16_t sustain_latch; // each bit is a MIDI channel's sustain state

    uint8_t pc_channel = 0; // program change channel filter, used for preset selection
    static constexpr uint8_t PC_OMNI = 0;

    PolyphonyData poly_buffer[DAC_CHANNEL_LAST]; // buffer for polyphonic data tracking
    uint8_t max_voice = 1;
    int poly_mode = 0;
    int8_t poly_rotate_index = -1;
    uint16_t midi_channel_filter = 0; // each bit state represents a channel. 1 means enabled. all 0's means Omni (no channel filter)
    bool any_channel_omni = false;

    void UpdateMidiChannelFilter() {
        uint16_t filter = 0;
        bool omni = false;
        for (int ch = 0; ch < MIDIMAP_MAX; ++ch) {
            MIDIMapping &map = mapping[ch];
            if (map.channel < 16) filter |= (1 << map.channel);
            else omni = true;
        }
        midi_channel_filter = filter;
        any_channel_omni = omni;
    }

    bool CheckMidiChannelFilter(const uint8_t m_ch) {
        return midi_channel_filter & (1 << m_ch);
    }

    void UpdateMaxPolyphony() { // find max voice number to determine how much to buffer
        int voice = 0;
        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            if (mapping[ch].dac_polyvoice > voice) voice = mapping[ch].dac_polyvoice;
        }
        if (max_voice != voice+1) {
            ClearPolyBuffer();
            max_voice = voice+1;
        }
    }

    bool CheckPolyVoice(const uint8_t voice) {
        return (poly_buffer[voice].gate);
    }

    int FindNextAvailPolyVoice(const uint8_t note) {
        if (max_voice == 1) return 0;

        switch (poly_mode) {
            case POLY_RESET:
                for (int v = 0; v < max_voice; ++v)
                    if (!CheckPolyVoice(v)) return v;
                return max_voice - 1;
                break;
            case POLY_REUSE:
                for (int v = 0; v < max_voice; ++v)
                    if (poly_buffer[v].note == note) return v;
                // fallthrough
            case POLY_ROTATE:
                for (int v = 0; v < max_voice; ++v) {
                    if (++poly_rotate_index >= max_voice) poly_rotate_index = 0;
                    if (!CheckPolyVoice(poly_rotate_index)) return poly_rotate_index;
                }
                // if no voices empty
                if (++poly_rotate_index >= max_voice) poly_rotate_index = 0;
                return poly_rotate_index;
                break;
            default:
                return 0;
        }
    }

    int FindPolyNoteIndex(const uint8_t note) {
        for (int v = 0; v < max_voice; ++v)
            if (poly_buffer[v].note == note) return v;
        return -1;
    }

    void WritePolyNoteData(const uint8_t note, const uint8_t vel, const uint8_t voice) {
        poly_buffer[voice].note = note;
        poly_buffer[voice].vel = vel;
        poly_buffer[voice].gate = 1;
    }

    void ClearPolyVoice(const uint8_t voice) {
        poly_buffer[voice].vel = 0;
        poly_buffer[voice].gate = 0;
    }

    void PolyBufferPush(const uint8_t m_ch, const uint8_t note, const uint8_t vel) {
        if (CheckMidiChannelFilter(m_ch) || any_channel_omni)
            WritePolyNoteData(note, vel, FindNextAvailPolyVoice(note));
    }

    void PolyBufferPop(const uint8_t m_ch, const uint8_t note) {
        if (CheckMidiChannelFilter(m_ch) || any_channel_omni) {
            for (uint8_t v = 0; v < max_voice; ++v) {
                if (poly_buffer[v].note == note) ClearPolyVoice(v);
            }
        }
    }

    void ClearPolyBuffer() {
        for (int ch = 0; ch < DAC_CHANNEL_LAST; ++ch) {
            ClearPolyVoice(ch);
        }
    }

    void RemoveNoteData(NoteBuffer &buffer, const uint8_t note) {
        buffer.erase(
            std::remove_if(buffer.begin(), buffer.end(), [&](MIDINoteData const &data) {
                return data.note == note;
            }),
            buffer.end()
        );
    }

    void MonoBufferPush(const uint8_t m_ch, const uint8_t note, const uint8_t vel) {
        if (CheckMidiChannelFilter(m_ch) || any_channel_omni) {
            RemoveNoteData(note_buffer[m_ch], note); // if new note is already in buffer, promote to latest and update velocity
            note_buffer[m_ch].push_back({note, vel}); // else just append to the end
        }
    }

    void MonoBufferPop(const uint8_t m_ch, const uint8_t note) {
        if (CheckMidiChannelFilter(m_ch) || any_channel_omni) {
            RemoveNoteData(note_buffer[m_ch], note);
            if (note_buffer[m_ch].size() == 0) note_buffer[m_ch].shrink_to_fit(); // free up memory when MIDI is not used
        }
    }

    void ClearMonoBuffer(const int8_t m_ch = -1) {
        if (m_ch > 0) {
            note_buffer[m_ch].clear();
            note_buffer[m_ch].shrink_to_fit();
        } else { // clear on all channels if no args passed
            for (uint8_t c = 0; c < 16; ++c) {
                note_buffer[c].clear();
                note_buffer[c].shrink_to_fit();
            }
        }
    }

    // int GetNote(std::vector<MIDINoteData> &buffer, const int n) {
    //     return buffer.at(buffer.size()-n).note;
    // }

    int GetNoteFirst(NoteBuffer &buffer) {
        return buffer.front().note;
    }

    int GetNoteLast(NoteBuffer &buffer) {
        return buffer.back().note;
    }

    int GetNoteLastInv(NoteBuffer &buffer) {
        return 127 - buffer.back().note;
    }

    int GetNoteMin(NoteBuffer &buffer) {
        uint8_t m = 127;
        std::for_each (buffer.begin(), buffer.end(), [&](MIDINoteData const &data) {
            if (data.note < m) m = data.note;
        });
        return m;
    }

    int GetNoteMax(NoteBuffer &buffer) {
        uint8_t m = 0;
        std::for_each (buffer.begin(), buffer.end(), [&](MIDINoteData const &data) {
            if (data.note > m) m = data.note;
        });
        return m;
    }

    int GetVel(NoteBuffer &buffer, const int n) {
        return buffer.at(buffer.size()-n).vel;
    }

    void ClearSustainLatch(int8_t m_ch = -1) {
        if (m_ch > 0) sustain_latch &= ~(1 << m_ch);
        else { // clear on all channels if no args passed
            for (uint8_t c = 0; c < 16; ++c)
                sustain_latch &= ~(1 << c);
        }
    }

    bool CheckSustainLatch(const uint8_t m_ch) {
        return sustain_latch & (1 << m_ch);
    }

    // Clock/Start/Stop are handled by ClockSetup applet
    bool clock_run = 0;
    bool clock_q;
    bool start_q;
    bool stop_q;
    uint8_t clock_count; // MIDI clock counter (24ppqn)
    uint32_t last_msg_tick; // Tick of last received message

    // MIDI output stuff
    int outchan[DAC_CHANNEL_LAST] = {
        0, 0, 1, 1,
#ifdef ARDUINO_TEENSY41
        2, 2, 3, 3,
#endif
    };
    int outchan_last[DAC_CHANNEL_LAST] = {
        0, 0, 1, 1,
#ifdef ARDUINO_TEENSY41
        2, 2, 3, 3,
#endif
    };
    int outfn[DAC_CHANNEL_LAST] = {
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
#ifdef ARDUINO_TEENSY41
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
        HEM_MIDI_NOTE_OUT, HEM_MIDI_GATE_OUT,
#endif
    };
    uint8_t outccnum[DAC_CHANNEL_LAST] = {
        1, 1, 1, 1,
#ifdef ARDUINO_TEENSY41
        5, 6, 7, 8,
#endif
    };
    uint8_t current_note[16]; // note number, per MIDI channel
    uint8_t current_ccval[DAC_CHANNEL_LAST]; // level 0 - 127, per DAC channel
    int note_countdown[DAC_CHANNEL_LAST];
    int inputs[DAC_CHANNEL_LAST]; // CV to be translated
    int last_cv[DAC_CHANNEL_LAST];
    bool clocked[DAC_CHANNEL_LAST];
    bool gate_high[DAC_CHANNEL_LAST];
    bool changed_cv[DAC_CHANNEL_LAST];

    // Logging
    MIDILogEntry log[7];
    int log_index;

    void UpdateLog(uint8_t message, uint8_t data1, uint8_t data2) {
        log[log_index++] = {message, data1, data2};
        if (log_index == 7) {
            for (int i = 0; i < 6; i++) {
                memcpy(&log[i], &log[i+1], sizeof(log[i+1]));
            }
            log_index--;
        }
        last_msg_tick = OC::CORE::ticks;
    }

    void ProcessMIDIMsg(const uint8_t midi_chan, const uint8_t message, const uint8_t data1, const uint8_t data2) {
        switch (message) {
            case usbMIDI.Clock:
                if (++clock_count == 1) {
                    clock_q = 1;
                    for(int ch = 0; ch < MIDIMAP_MAX; ++ch) {
                        if (mapping[ch].function == HEM_MIDI_CLOCK_OUT) {
                            mapping[ch].trigout_q = 1;
                        }
                    }
                }
                if (clock_count == HEM_MIDI_CLOCK_DIVISOR) clock_count = 0;
                return;
                break;

            case usbMIDI.Continue: // treat Continue like Start
            case usbMIDI.Start:
                start_q = 1;
                clock_count = 0;
                clock_run = true;

                for(int ch = 0; ch < MIDIMAP_MAX; ++ch) {
                    if (mapping[ch].function == HEM_MIDI_START_OUT) {
                        mapping[ch].trigout_q = 1;
                    }
                }

                // UpdateLog(message, data1, data2);
                return;
                break;

            case usbMIDI.SystemReset:
            case usbMIDI.Stop:
                stop_q = 1;
                clock_run = false;
                // a way to reset stuck notes
                ClearMonoBuffer();
                ClearSustainLatch();
                ClearPolyBuffer();
                for (int ch = 0; ch < MIDIMAP_MAX; ++ch) {
                    mapping[ch].output = 0;
                    mapping[ch].trigout_q = 0;
                }
                return;
                break;

            case usbMIDI.NoteOn:
                MonoBufferPush(midi_chan-1, data1, data2);
                PolyBufferPush(midi_chan-1, data1, data2);
                break;

            case usbMIDI.NoteOff:
                MonoBufferPop(midi_chan-1, data1);
                PolyBufferPop(midi_chan-1, data1);
                break;
        }

        bool log_skip = false;
        uint8_t m_ch_prev = 255; // initialize to invalid channel

        for(int ch = 0; ch < MIDIMAP_MAX; ++ch) {
            MIDIMapping &map = mapping[ch];
            if (map.function == HEM_MIDI_NOOP) continue;

            // skip unwanted MIDI Channels
            uint8_t m_ch = midi_chan - 1;
            if (map.channel != m_ch && map.channel != 16) continue;

            last_midi_channel = m_ch;

            // prevent duplicate log entries
            if (m_ch == m_ch_prev) log_skip = true;
            else log_skip = false;
            m_ch_prev = m_ch;

            bool log_this = false;

            switch (message) {
                case usbMIDI.NoteOn: {
                    map.semitone_mask = map.semitone_mask | (1u << (data1 % 12));

                    // Should this message go out on this channel?
                    switch (map.function) { // note # output functions
                        case HEM_MIDI_NOTE_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteLast(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_POLY_OUT:
                            if (CheckPolyVoice(map.dac_polyvoice)) map.output = MIDIQuantizer::CV(poly_buffer[map.dac_polyvoice].note);
                            break;

                        case HEM_MIDI_NOTE_MIN_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteMin(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_MAX_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteMax(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_PEDAL_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteFirst(note_buffer[m_ch]));
                            break;

                        case HEM_MIDI_NOTE_INV_OUT:
                            map.output = MIDIQuantizer::CV(GetNoteLastInv(note_buffer[m_ch]));
                            break;
                    }

                    if ((map.function == HEM_MIDI_TRIG_OUT)
                    ||  (map.function == HEM_MIDI_TRIG_ALWAYS_OUT)
                    || ((map.function == HEM_MIDI_TRIG_1ST_OUT) && (note_buffer[m_ch].size() == 1)))
                        map.trigout_q = 1;

                    if (map.function == HEM_MIDI_GATE_OUT)
                        map.output = PULSE_VOLTAGE * (12 << 7);
                    if (map.function == HEM_MIDI_GATE_INV_OUT)
                        map.output = 0;
                    if (map.function == HEM_MIDI_GATE_POLY_OUT)
                            if (CheckPolyVoice(map.dac_polyvoice)) map.output = PULSE_VOLTAGE * (12 << 7);

                    if (map.function == HEM_MIDI_VEL_OUT)
                        map.output = (note_buffer[m_ch].size() > 0) ? Proportion(GetVel(note_buffer[m_ch], 1), 127, HEMISPHERE_MAX_CV) : 0;
                    if (map.function == HEM_MIDI_VEL_POLY_OUT) {
                        map.output = (CheckPolyVoice(map.dac_polyvoice)) ? Proportion(poly_buffer[map.dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;
                    }

                    if (!log_skip) log_this = 1; // Log all MIDI notes. Other stuff is conditional.
                    break;
                }
                case usbMIDI.NoteOff: {
                    map.semitone_mask = map.semitone_mask & ~(1u << (data1 % 12));

                    if (note_buffer[m_ch].size() > 0) { // don't update output when last note is released
                        switch(map.function) { // note # output functions
                            case HEM_MIDI_NOTE_OUT:
                                if (CheckSustainLatch(m_ch)) break;
                                map.output = MIDIQuantizer::CV(GetNoteLast(note_buffer[m_ch]));
                                break;

                            case HEM_MIDI_NOTE_POLY_OUT:
                                if (CheckSustainLatch(m_ch)) break;
                                if (CheckPolyVoice(map.dac_polyvoice)) map.output = MIDIQuantizer::CV(poly_buffer[map.dac_polyvoice].note);
                                break;

                            case HEM_MIDI_NOTE_MIN_OUT:
                                if (CheckSustainLatch(m_ch)) break;
                                map.output = MIDIQuantizer::CV(GetNoteMin(note_buffer[m_ch]));
                                break;

                            case HEM_MIDI_NOTE_MAX_OUT:
                                if (CheckSustainLatch(m_ch)) break;
                                map.output = MIDIQuantizer::CV(GetNoteMax(note_buffer[m_ch]));
                                break;

                            case HEM_MIDI_NOTE_PEDAL_OUT:
                                if (CheckSustainLatch(m_ch)) break;
                                map.output = MIDIQuantizer::CV(GetNoteFirst(note_buffer[m_ch]));
                                break;

                            case HEM_MIDI_NOTE_INV_OUT:
                                if (CheckSustainLatch(m_ch)) break;
                                map.output = MIDIQuantizer::CV(GetNoteLastInv(note_buffer[m_ch]));
                                break;
                        }
                    }

                    if (map.function == HEM_MIDI_TRIG_ALWAYS_OUT) map.trigout_q = 1;

                    if (!CheckSustainLatch(m_ch)) {
                        if (!(note_buffer[m_ch].size() > 0)) { // turn mono gate off, only when all notes are off
                            if (map.function == HEM_MIDI_GATE_OUT) map.output = 0;
                            if (map.function == HEM_MIDI_GATE_INV_OUT) map.output = PULSE_VOLTAGE * (12 << 7);
                        }
                        if (map.function == HEM_MIDI_GATE_POLY_OUT) {
                            if (!CheckPolyVoice(map.dac_polyvoice)) map.output = 0;
                        }
                    }

                    if (map.function == HEM_MIDI_VEL_OUT)
                        map.output = (note_buffer[m_ch].size() > 0) ? Proportion(GetVel(note_buffer[m_ch], 1), 127, HEMISPHERE_MAX_CV) : 0;
                    if (map.function == HEM_MIDI_VEL_POLY_OUT)
                        map.output = (CheckPolyVoice(map.dac_polyvoice)) ? Proportion(poly_buffer[map.dac_polyvoice].vel, 127, HEMISPHERE_MAX_CV) : 0;

                    if (!log_skip) log_this = 1;
                    break;
                }
                case usbMIDI.ControlChange: { // Modulation wheel or other CC
                    // handle sustain pedal
                    if (data1 == 64) {
                        if (data2 > 63) {
                            if (!CheckSustainLatch(m_ch)) sustain_latch |= (1 << m_ch);
                        } else {
                            ClearSustainLatch(m_ch);
                            if (!(note_buffer[m_ch].size() > 0)) {
                                switch (map.function) {
                                    case HEM_MIDI_GATE_OUT:
                                    case HEM_MIDI_GATE_POLY_OUT:
                                        map.output = 0;
                                        break;
                                }
                                if (map.function == HEM_MIDI_GATE_INV_OUT) {
                                    map.output = PULSE_VOLTAGE * (12 << 7);
                                }
                            }
                        }
                    }

                    if (map.function == HEM_MIDI_CC_OUT) {
                        if (map.function_cc < 0) map.function_cc = data1;

                        if (map.function_cc == data1) {
                            map.output = Proportion(data2, 127, HEMISPHERE_MAX_CV);
                            if (!log_skip) log_this = 1;
                        }
                    }
                    break;
                }
                case usbMIDI.AfterTouchPoly: {
                    if (map.function == HEM_MIDI_AT_KEY_POLY_OUT) {
                        if (FindPolyNoteIndex(data1) == map.dac_polyvoice)
                            map.output = Proportion(data2, 127, HEMISPHERE_MAX_CV);
                        if (!log_skip) log_this = 1;
                    }
                    break;
                }
                case usbMIDI.AfterTouchChannel: {
                    if (map.function == HEM_MIDI_AT_CHAN_OUT) {
                        map.output = Proportion(data1, 127, HEMISPHERE_MAX_CV);
                        if (!log_skip) log_this = 1;
                    }
                    break;
                }
                case usbMIDI.PitchBend: {
                    if (map.function == HEM_MIDI_PB_OUT) {
                        int data = (data2 << 7) + data1 - 8192;
                        map.output = Proportion(data, 8192, HEMISPHERE_3V_CV);
                        if (!log_skip) log_this = 1;
                    }
                    break;
                }
            }
            if (log_this) UpdateLog(message, data1, data2);
        }
    }

    void Send(const int *outvals) {
        // first pass - calculate things and turn off notes
        for (int i = 0; i < DAC_CHANNEL_LAST; ++i) {
            const uint8_t midi_ch = outchan[i];

            inputs[i] = outvals[i];
            gate_high[i] = inputs[i] > (12 << 7);
            clocked[i] = (gate_high[i] && last_cv[i] < (12 << 7));
            if (abs(inputs[i] - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
                changed_cv[i] = 1;
                last_cv[i] = inputs[i];
            } else changed_cv[i] = 0;

            switch (outfn[i]) {
                case HEM_MIDI_NOTE_OUT:
                    if (changed_cv[i]) {
                        // a note has changed, turn the last one off first
                        SendNoteOff(outchan_last[i]);
                        current_note[midi_ch] = MIDIQuantizer::NoteNumber( inputs[i] );
                    }
                    break;

                case HEM_MIDI_GATE_OUT:
                    if (!gate_high[i] && changed_cv[i])
                        SendNoteOff(midi_ch);
                    break;

                case HEM_MIDI_CC_OUT:
                {
                    const uint8_t newccval = ProportionCV(abs(inputs[i]), 127);
                    if (newccval != current_ccval[i])
                        SendCC(midi_ch, outccnum[i], newccval);
                    current_ccval[i] = newccval;
                    break;
                }
            }

            // Handle clock pulse timing
            if (note_countdown[i] > 0) {
                if (--note_countdown[i] == 0) SendNoteOff(outchan_last[i]);
            }
        }

        // 2nd pass - send eligible notes
        for (int i = 0; i < 2; ++i) {
            const int chA = i*2;
            const int chB = chA + 1;

            if (outfn[chB] == HEM_MIDI_GATE_OUT) {
                if (clocked[chB]) {
                    SendNoteOn(outchan[chB]);
                    // no countdown
                    outchan_last[chB] = outchan[chB];
                }
            } else if (outfn[chA] == HEM_MIDI_NOTE_OUT) {
                if (changed_cv[chA]) {
                    SendNoteOn(outchan[chA]);
                    note_countdown[chA] = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
                    outchan_last[chA] = outchan[chA];
                }
            }
        }

        // I think this can cause the UI to lag and miss input
        //usbMIDI.send_now();
    }

    void SendAfterTouch(const uint8_t midi_ch, uint8_t val) {
        usbMIDI.sendAfterTouch(val, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendAfterTouch(val, midi_ch + 1);
        MIDI1.sendAfterTouch(val, midi_ch + 1);
#endif
    }
    void SendPitchBend(const uint8_t midi_ch, uint16_t bend) {
        usbMIDI.sendPitchBend(bend, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendPitchBend(bend, midi_ch + 1);
        MIDI1.sendPitchBend(bend, midi_ch + 1);
#endif
    }

    void SendCC(const uint8_t midi_ch, uint8_t ccnum, uint8_t val) {
        usbMIDI.sendControlChange(ccnum, val, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendControlChange(ccnum, val, midi_ch + 1);
        MIDI1.sendControlChange(ccnum, val, midi_ch + 1);
#endif
    }
    void SendNoteOn(const uint8_t midi_ch, uint8_t note = 255, uint8_t vel = 100) {
        if (note > 127) note = current_note[midi_ch];
        else current_note[midi_ch] = note;

        usbMIDI.sendNoteOn(note, vel, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendNoteOn(note, vel, midi_ch + 1);
        MIDI1.sendNoteOn(note, vel, midi_ch + 1);
#endif
    }
    void SendNoteOff(const uint8_t midi_ch, uint8_t note = 255, uint8_t vel = 0) {
        if (note > 127) note = current_note[midi_ch];
        usbMIDI.sendNoteOff(note, vel, midi_ch + 1);
#ifdef ARDUINO_TEENSY41
        usbHostMIDI.sendNoteOff(note, vel, midi_ch + 1);
        MIDI1.sendNoteOff(note, vel, midi_ch + 1);
#endif
    }
};

// shared IO Frame, updated every tick
// this will allow chaining applets together, multiple stages of processing
struct IOFrame {
    bool autoMIDIOut = false;
    bool clocked[OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST];
    bool gate_high[OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST];
    int inputs[ADC_CHANNEL_LAST];
    int outputs[DAC_CHANNEL_LAST];
    int output_diff[DAC_CHANNEL_LAST];
    int outputs_smooth[DAC_CHANNEL_LAST];
    int clock_countdown[DAC_CHANNEL_LAST];
    uint8_t clockskip[DAC_CHANNEL_LAST] = {0};
    bool clockout_q[DAC_CHANNEL_LAST]; // for loopback
    int adc_lag_countdown[ADC_CHANNEL_LAST]; // Time between a clock event and an ADC read event
    uint32_t last_clock[ADC_CHANNEL_LAST]; // Tick number of the last clock observed by the child class
    uint32_t cycle_ticks[ADC_CHANNEL_LAST]; // Number of ticks between last two clocks
    bool changed_cv[ADC_CHANNEL_LAST]; // Has the input changed by more than 1/8 semitone since the last read?
    int last_cv[ADC_CHANNEL_LAST]; // For change detection

    /* MIDI message queue/cache */
    MIDIFrame MIDIState;

    // --- Soft IO ---
    void Out(DAC_CHANNEL channel, int value) {
        // rising edge detection for trigger loopback
        if (value > GATE_THRESHOLD && outputs[channel] < GATE_THRESHOLD)
            clockout_q[channel] = true;

        output_diff[channel] += value - outputs[channel];
        outputs[channel] = value;
    }
    void ClockOut(DAC_CHANNEL ch, const int pulselength = HEMISPHERE_CLOCK_TICKS * HS::trig_length) {
        // short circuit if skip probability is zero to avoid consuming random numbers
        if (0 == clockskip[ch] || random(100) >= clockskip[ch]) {
            clock_countdown[ch] = pulselength;
            outputs[ch] = PULSE_VOLTAGE * (12 << 7);
            clockout_q[ch] = true;
        }
    }
    void NudgeSkip(int ch, int dir) {
        clockskip[ch] = constrain(clockskip[ch] + dir, 0, 100);
    }

    // TODO: Hardware IO should be extracted
    // --- Hard IO ---
    void Load() {
        bool clocktmp[OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST];
        clocktmp[0] = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_1>();
        clocktmp[1] = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_2>();
        clocktmp[2] = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_3>();
        clocktmp[3] = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_4>();
        gate_high[0] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_1>();
        gate_high[1] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_2>();
        gate_high[2] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_3>();
        gate_high[3] = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_4>();
        for (int i = 0; i < ADC_CHANNEL_LAST; ++i) {
            // Set CV inputs
            inputs[i] = OC::ADC::raw_pitch_value(ADC_CHANNEL(i));

            // calculate gates/clocks for all ADC inputs as well
            gate_high[OC::DIGITAL_INPUT_LAST + i] = inputs[i] > GATE_THRESHOLD;
            clocktmp[OC::DIGITAL_INPUT_LAST + i] = (gate_high[OC::DIGITAL_INPUT_LAST + i] && last_cv[i] < GATE_THRESHOLD);

            if (abs(inputs[i] - last_cv[i]) > HEMISPHERE_CHANGE_THRESHOLD) {
                changed_cv[i] = 1;
                last_cv[i] = inputs[i];
            } else changed_cv[i] = 0;

            // Handle clock pulse timing
            if (clock_countdown[i] > 0) {
                if (--clock_countdown[i] == 0) outputs[i] = 0;
            }
        }

        // pre-calculate clock triggers
        static constexpr int offset = OC::DIGITAL_INPUT_LAST + ADC_CHANNEL_LAST;
        for (int ch = 0; ch < APPLET_SLOTS * 2; ++ch) {
          bool result = 0;
          const size_t virt_chan = (ch) % (APPLET_SLOTS * 2);
          const int trmap = trigger_mapping[ch];

          // clock triggers
          if (clock_m.IsRunning() && clock_m.GetMultiply(virt_chan) != 0)
              result = clock_m.Tock(virt_chan);
          else if (trmap > 0) {
            if (trmap <= offset)
              result = clocktmp[ trmap - 1 ];
            else {
              result = clockout_q[ trmap - 1 - offset ];
            }
          }

          // Try to eat a boop
          result = result || clock_m.Beep(virt_chan);

          if (result) {
              cycle_ticks[ch] = OC::CORE::ticks - last_clock[ch];
              last_clock[ch] = OC::CORE::ticks;
          }

          clocked[ch] = result;
        }
        for (int i = 0; i < DAC_CHANNEL_LAST; ++i) {
          clockout_q[i] = false;
        }
    }

    void Send() {
        const DAC_CHANNEL chan[DAC_CHANNEL_LAST] = {
          DAC_CHANNEL_A, DAC_CHANNEL_B, DAC_CHANNEL_C, DAC_CHANNEL_D,
#ifdef ARDUINO_TEENSY41
          DAC_CHANNEL_E, DAC_CHANNEL_F, DAC_CHANNEL_G, DAC_CHANNEL_H,
#endif
        };
        for (int i = 0; i < DAC_CHANNEL_LAST; ++i) {
            OC::DAC::set_pitch_scaled(chan[i], outputs[i], 0);
        }
        if (autoMIDIOut) MIDIState.Send(outputs);
    }

};

} // namespace HS

