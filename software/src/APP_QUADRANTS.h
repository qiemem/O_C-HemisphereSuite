// Copyright (c) 2018, Jason Justian
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

#pragma once

#include "OC_DAC.h"
#include "OC_digital_inputs.h"
#include "OC_visualfx.h"
#include "OC_apps.h"
#include "OC_ui.h"

#include "OC_patterns.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

#include "HemisphereApplet.h"
#include "HSApplication.h"
#include "HSicons.h"
#include "HSMIDI.h"
#include "HSClockManager.h"
#include "AudioSetup.h"

#include "hemisphere_config.h"
#include "hemisphere_audio_config.h"

// We depend on Calibr8or now
#include "APP_CALIBR8OR.h"

// The settings specify the selected applets, and 64 bits of data for each applet,
// plus 64 bits of data for the ClockSetup applet (which includes some misc config).
// This is the structure of a QuadrantsPreset in eeprom.
enum QUADRANTS_SETTINGS {
    QUADRANTS_SELECTED_LEFT_ID = 0,
    QUADRANTS_SELECTED_RIGHT_ID = 1,
    QUADRANTS_SELECTED_LEFT2_ID = 2,
    QUADRANTS_SELECTED_RIGHT2_ID = 3,
    QUADRANTS_LEFT_DATA_B1,
    QUADRANTS_LEFT_DATA_B2,
    QUADRANTS_LEFT_DATA_B3,
    QUADRANTS_LEFT_DATA_B4,
    QUADRANTS_RIGHT_DATA_B1,
    QUADRANTS_RIGHT_DATA_B2,
    QUADRANTS_RIGHT_DATA_B3,
    QUADRANTS_RIGHT_DATA_B4,
    QUADRANTS_LEFT2_DATA_B1,
    QUADRANTS_LEFT2_DATA_B2,
    QUADRANTS_LEFT2_DATA_B3,
    QUADRANTS_LEFT2_DATA_B4,
    QUADRANTS_RIGHT2_DATA_B1,
    QUADRANTS_RIGHT2_DATA_B2,
    QUADRANTS_RIGHT2_DATA_B3,
    QUADRANTS_RIGHT2_DATA_B4,
    QUADRANTS_CLOCK_DATA1,
    QUADRANTS_CLOCK_DATA2,
    QUADRANTS_CLOCK_DATA3,
    QUADRANTS_CLOCK_DATA4,
    QUADRANTS_TRIGMAP1, // 3 x 5-bit values
    QUADRANTS_TRIGMAP2,
    QUADRANTS_TRIGMAP3,
    QUADRANTS_CVMAP1, // 3 x 5-bit values
    QUADRANTS_CVMAP2,
    QUADRANTS_CVMAP3,
    QUADRANTS_GLOBALS1, // for globals like trig_length
    QUADRANTS_GLOBALS2,
    QUADRANTS_SETTING_LAST
};

static constexpr int QUAD_PRESET_COUNT = 4;

/* Preset
 * - conveniently store/recall multiple applet configurations
 */
class QuadrantsPreset : public SystemExclusiveHandler,
    public settings::SettingsBase<QuadrantsPreset, QUADRANTS_SETTING_LAST> {
public:
    int GetAppletId(HEM_SIDE h) {
        return values_[QUADRANTS_SELECTED_LEFT_ID + h];
    }
    HemisphereApplet* GetApplet(HEM_SIDE h) {
      int idx = HS::get_applet_index_by_id( GetAppletId(h) );
      return HS::available_applets[idx].instance[h];
    }
    void SetAppletId(HEM_SIDE h, int id) {
        apply_value(h, id);
    }
    bool is_valid() {
        return values_[QUADRANTS_SELECTED_LEFT_ID] != 0;
    }

    uint64_t GetClockData() {
        return ( (uint64_t(values_[QUADRANTS_CLOCK_DATA4]) << 48) |
                 (uint64_t(values_[QUADRANTS_CLOCK_DATA3]) << 32) |
                 (uint64_t(values_[QUADRANTS_CLOCK_DATA2]) << 16) |
                  uint64_t(values_[QUADRANTS_CLOCK_DATA1]) );
    }
    void SetClockData(const uint64_t data) {
        apply_value(QUADRANTS_CLOCK_DATA1, data & 0xffff);
        apply_value(QUADRANTS_CLOCK_DATA2, (data >> 16) & 0xffff);
        apply_value(QUADRANTS_CLOCK_DATA3, (data >> 32) & 0xffff);
        apply_value(QUADRANTS_CLOCK_DATA4, (data >> 48) & 0xffff);
    }

    // returns true if changed
    bool StoreInputMap() {
      // TODO: cvmap
      uint64_t trigmap = 0;
      uint64_t cvmap = 0;
      for (size_t i = 0; i < 8; ++i) {
        trigmap |= (uint64_t(HS::trigger_mapping[i] + 1) & 0x1F) << (i*5 + i/3);
        cvmap |= (uint64_t(HS::cvmapping[i] + 1) & 0x1F) << (i*5 + i/3);
      }

      bool changed = (
          trigmap != ( uint64_t(values_[QUADRANTS_TRIGMAP1])
                    | (uint64_t(values_[QUADRANTS_TRIGMAP2]) << 16)
                    | (uint64_t(values_[QUADRANTS_TRIGMAP3]) << 32) )
          ) || (
          cvmap   != ( uint64_t(values_[QUADRANTS_CVMAP1])
                    | (uint64_t(values_[QUADRANTS_CVMAP2]) << 16)
                    | (uint64_t(values_[QUADRANTS_CVMAP3]) << 32) )
          );
      values_[QUADRANTS_TRIGMAP1] = trigmap & 0xFFFF;
      values_[QUADRANTS_TRIGMAP2] = (trigmap >> 16) & 0xFFFF;
      values_[QUADRANTS_TRIGMAP3] = (trigmap >> 32) & 0xFFFF;
      values_[QUADRANTS_CVMAP1] = cvmap & 0xFFFF;
      values_[QUADRANTS_CVMAP2] = (cvmap >> 16) & 0xFFFF;
      values_[QUADRANTS_CVMAP3] = (cvmap >> 32) & 0xFFFF;
      return changed;
    }
    void LoadInputMap() {
      int val;
      for (size_t i = 0; i < 8; ++i) {
        val = (uint32_t(values_[QUADRANTS_TRIGMAP1 + i/3]) >> (i%3 * 5)) & 0x1F;
        if (val != 0) HS::trigger_mapping[i] = constrain(val - 1, 0, TRIGMAP_MAX);

        val = (uint32_t(values_[QUADRANTS_CVMAP1 + i/3]) >> (i%3 * 5)) & 0x1F;
        if (val != 0) HS::cvmapping[i] = constrain(val - 1, 0, CVMAP_MAX);
      }
    }

    uint64_t GetGlobals() {
      return ( (uint64_t(values_[QUADRANTS_GLOBALS2]) << 16) |
                uint64_t(values_[QUADRANTS_GLOBALS1]) );
    }
    void SetGlobals(uint64_t &data) {
        apply_value(QUADRANTS_GLOBALS1, data & 0xffff);
        apply_value(QUADRANTS_GLOBALS2, (data >> 16) & 0xffff);
    }

    // Manually get data for one side
    uint64_t GetData(const HEM_SIDE h) {
        const size_t offset = h*4;
        return (uint64_t(values_[7 + offset]) << 48) |
               (uint64_t(values_[6 + offset]) << 32) |
               (uint64_t(values_[5 + offset]) << 16) |
               (uint64_t(values_[4 + offset]));
    }

    /* Manually store state data for one side */
    void SetData(const HEM_SIDE h, uint64_t &data) {
        apply_value(4 + h*4, data & 0xffff);
        apply_value(5 + h*4, (data >> 16) & 0xffff);
        apply_value(6 + h*4, (data >> 32) & 0xffff);
        apply_value(7 + h*4, (data >> 48) & 0xffff);
    }

    // TODO: I haven't updated the SysEx data structure here because I don't use it.
    // Clock data would probably be useful if it's not too big. -NJM
    void OnSendSysEx() {
        // Describe the data structure for the audience
        uint8_t V[18];
        V[0] = (uint8_t)values_[QUADRANTS_SELECTED_LEFT_ID];
        V[1] = (uint8_t)values_[QUADRANTS_SELECTED_RIGHT_ID];
        V[2] = (uint8_t)(values_[QUADRANTS_LEFT_DATA_B1] & 0xff);
        V[3] = (uint8_t)((values_[QUADRANTS_LEFT_DATA_B1] >> 8) & 0xff);
        V[4] = (uint8_t)(values_[QUADRANTS_RIGHT_DATA_B1] & 0xff);
        V[5] = (uint8_t)((values_[QUADRANTS_RIGHT_DATA_B1] >> 8) & 0xff);
        V[6] = (uint8_t)(values_[QUADRANTS_LEFT_DATA_B2] & 0xff);
        V[7] = (uint8_t)((values_[QUADRANTS_LEFT_DATA_B2] >> 8) & 0xff);
        V[8] = (uint8_t)(values_[QUADRANTS_RIGHT_DATA_B2] & 0xff);
        V[9] = (uint8_t)((values_[QUADRANTS_RIGHT_DATA_B2] >> 8) & 0xff);
        V[10] = (uint8_t)(values_[QUADRANTS_LEFT_DATA_B3] & 0xff);
        V[11] = (uint8_t)((values_[QUADRANTS_LEFT_DATA_B3] >> 8) & 0xff);
        V[12] = (uint8_t)(values_[QUADRANTS_RIGHT_DATA_B3] & 0xff);
        V[13] = (uint8_t)((values_[QUADRANTS_RIGHT_DATA_B3] >> 8) & 0xff);
        V[14] = (uint8_t)(values_[QUADRANTS_LEFT_DATA_B4] & 0xff);
        V[15] = (uint8_t)((values_[QUADRANTS_LEFT_DATA_B4] >> 8) & 0xff);
        V[16] = (uint8_t)(values_[QUADRANTS_RIGHT_DATA_B4] & 0xff);
        V[17] = (uint8_t)((values_[QUADRANTS_RIGHT_DATA_B4] >> 8) & 0xff);

        // Pack it up, ship it out
        UnpackedData unpacked;
        unpacked.set_data(18, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'H');
    }

    void OnReceiveSysEx() {
        uint8_t V[18];
        if (ExtractSysExData(V, 'H')) {
            values_[QUADRANTS_SELECTED_LEFT_ID] = V[0];
            values_[QUADRANTS_SELECTED_RIGHT_ID] = V[1];
            values_[QUADRANTS_LEFT_DATA_B1] = ((uint16_t)V[3] << 8) + V[2];
            values_[QUADRANTS_RIGHT_DATA_B1] = ((uint16_t)V[5] << 8) + V[4];
            values_[QUADRANTS_LEFT_DATA_B2] = ((uint16_t)V[7] << 8) + V[6];
            values_[QUADRANTS_RIGHT_DATA_B2] = ((uint16_t)V[9] << 8) + V[8];
            values_[QUADRANTS_LEFT_DATA_B3] = ((uint16_t)V[11] << 8) + V[10];
            values_[QUADRANTS_RIGHT_DATA_B3] = ((uint16_t)V[13] << 8) + V[12];
            values_[QUADRANTS_LEFT_DATA_B4] = ((uint16_t)V[15] << 8) + V[14];
            values_[QUADRANTS_RIGHT_DATA_B4] = ((uint16_t)V[17] << 8) + V[16];
        }
    }

};

QuadrantsPreset quad_presets[QUAD_PRESET_COUNT];
QuadrantsPreset *quad_active_preset = 0;

////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Manager
////////////////////////////////////////////////////////////////////////////////

using namespace HS;

void QuadrantSysExHandler();
void QuadrantBeatSync();

class QuadAppletManager : public HSApplication {
public:
    void Start() {
        audio_app.SetParentApp(this);

        for (int i = 0; i < 4; ++i) {
            quant_scale[i] = OC::Scales::SCALE_SEMI;
            quantizer[i].Init();
            quantizer[i].Configure(OC::Scales::GetScale(quant_scale[i]), 0xffff);
        }

        SetApplet(HEM_SIDE(0), HS::get_applet_index_by_id(18)); // DualTM
        SetApplet(HEM_SIDE(1), HS::get_applet_index_by_id(15)); // EuclidX
        SetApplet(HEM_SIDE(2), HS::get_applet_index_by_id(68)); // DivSeq
        SetApplet(HEM_SIDE(3), HS::get_applet_index_by_id(71)); // Pigeons
    }

    void Resume() {
        audio_app.SetParentApp(this);
        if (!quad_active_preset)
            LoadFromPreset(0);
        // TODO: restore quantizer settings...
    }
    void Suspend() {
        if (quad_active_preset) {
            if (HS::auto_save_enabled || 0 == preset_id) StoreToPreset(preset_id, !HS::auto_save_enabled);
            quad_active_preset->OnSendSysEx();
        }
    }

    void StoreToPreset(QuadrantsPreset* preset, bool skip_eeprom = false) {
        bool doSave = (preset != quad_active_preset);

        quad_active_preset = preset;
        for (int h = 0; h < APPLET_SLOTS; h++)
        {
            int index = active_applet_index[h];
            if (quad_active_preset->GetAppletId(HEM_SIDE(h)) != HS::available_applets[index].id)
                doSave = 1;
            quad_active_preset->SetAppletId(HEM_SIDE(h), HS::available_applets[index].id);

            uint64_t data = HS::available_applets[index].instance[h]->OnDataRequest();
            if (data != applet_data[h]) doSave = 1;
            applet_data[h] = data;
            quad_active_preset->SetData(HEM_SIDE(h), data);
        }
        uint64_t data = ClockSetup_instance.OnDataRequest();
        if (data != clock_data) doSave = 1;
        clock_data = data;
        quad_active_preset->SetClockData(data);

        data = ClockSetup_instance.GetGlobals();
        if (data != global_data) doSave = 1;
        global_data = data;
        quad_active_preset->SetGlobals(data);

        if (quad_active_preset->StoreInputMap()) doSave = 1;

        // initiate actual EEPROM save - ONLY if necessary!
        if (doSave && !skip_eeprom) {
          Calibr8or_instance.SavePreset();
        }

    }
    void StoreToPreset(int id, bool skip_eeprom = false) {
        StoreToPreset( (QuadrantsPreset*)(quad_presets + id), skip_eeprom );
        preset_id = id;
    }
    void LoadFromPreset(int id) {
        quad_active_preset = (QuadrantsPreset*)(quad_presets + id);
        if (quad_active_preset->is_valid()) {
            clock_data = quad_active_preset->GetClockData();
            ClockSetup_instance.OnDataReceive(clock_data);

            global_data = quad_active_preset->GetGlobals();
            ClockSetup_instance.SetGlobals(global_data);

            quad_active_preset->LoadInputMap();

            for (int h = 0; h < APPLET_SLOTS; h++)
            {
                int index = HS::get_applet_index_by_id( quad_active_preset->GetAppletId(HEM_SIDE(h)) );
                applet_data[h] = quad_active_preset->GetData(HEM_SIDE(h));
                SetApplet(HEM_SIDE(h), index);
                HS::available_applets[index].instance[h]->OnDataReceive(applet_data[h]);
            }
        }
        preset_id = id;
        PokePopup(PRESET_POPUP);
    }
    void ProcessQueue() {
      LoadFromPreset(queued_preset);
    }
    void QueuePresetLoad(int id) {
      if (HS::clock_m.IsRunning()) {
        queued_preset = id;
        HS::clock_m.BeatSync( &QuadrantBeatSync );
      }
      else
        LoadFromPreset(id);
    }

    // does not modify the preset, only the quad_manager
    void SetApplet(HEM_SIDE hemisphere, int index) {
        if (active_applet[hemisphere])
          active_applet[hemisphere]->Unload();

        next_applet_index[hemisphere] = active_applet_index[hemisphere] = index;
        active_applet[hemisphere] = HS::available_applets[index].instance[hemisphere];
        active_applet[hemisphere]->BaseStart(hemisphere);
    }
    void ChangeApplet(HEM_SIDE h, int dir) {
        int index = HS::get_next_applet_index(next_applet_index[h], dir);
        next_applet_index[h] = index;
    }

    template <typename T1, typename T2, typename T3>
    void ProcessMIDI(T1 &device, T2 &next_device, T3 &dev3) {
        while (device.read()) {
            const int message = device.getType();
            const int data1 = device.getData1();
            const int data2 = device.getData2();

            if (message == usbMIDI.SystemExclusive) {
                QuadrantSysExHandler();
                continue;
            }

            if (message == usbMIDI.ProgramChange) {
                int slot = device.getData1();
                if (slot < QUAD_PRESET_COUNT) {
                  QueuePresetLoad(slot);
                }
                continue;
            }

            HS::frame.MIDIState.ProcessMIDIMsg(device.getChannel(), message, data1, data2);
            next_device.send(message, data1, data2, device.getChannel(), 0);
            dev3.send((midi::MidiType)message, data1, data2, device.getChannel());
        }
    }

    void Controller() {
        // top-level MIDI-to-CV handling - alters frame outputs
        ProcessMIDI(usbMIDI, usbHostMIDI, MIDI1);
        thisUSB.Task();
        ProcessMIDI(usbHostMIDI, usbMIDI, MIDI1);
        ProcessMIDI(MIDI1, usbMIDI, usbHostMIDI);

        // Clock Setup applet handles internal clock duties
        ClockSetup_instance.Controller();

        // execute Applets
        for (int h = 0; h < APPLET_SLOTS; h++)
        {
            if (active_applet_index[h] != next_applet_index[h]) {
              SetApplet(HEM_SIDE(h), next_applet_index[h]);
            }

            // MIDI signals mixed with inputs to applets
            if (HS::available_applets[ active_applet_index[h] ].id != 150) // not MIDI In
            {
                ForEachChannel(ch) {
                    int chan = h*2 + ch;
                    // mix CV inputs with applicable MIDI signals
                    switch (HS::frame.MIDIState.function[chan]) {
                    case HEM_MIDI_CC_OUT:
                    case HEM_MIDI_NOTE_OUT:
                    case HEM_MIDI_VEL_OUT:
                    case HEM_MIDI_AT_OUT:
                    case HEM_MIDI_PB_OUT:
                        HS::frame.inputs[chan] += HS::frame.MIDIState.outputs[chan];
                        break;
                    case HEM_MIDI_GATE_OUT:
                        HS::frame.gate_high[chan] |= (HS::frame.MIDIState.outputs[chan] > (12 << 7));
                        break;
                    case HEM_MIDI_TRIG_OUT:
                    case HEM_MIDI_CLOCK_OUT:
                    case HEM_MIDI_START_OUT:
                        HS::frame.clocked[chan] |= HS::frame.MIDIState.trigout_q[chan];
                        HS::frame.MIDIState.trigout_q[chan] = 0;
                        break;
                    }
                }
            }
            active_applet[h]->BaseController();
        }
    }

    void View() {
        bool draw_applets = true;

        if (preset_cursor) {
          DrawPresetSelector();
          draw_applets = false;
        }
        else if (config_page > HIDE_CONFIG) {
          switch(config_page) {
          default:
          case LOADSAVE_POPUP:
            PokePopup(MENU_POPUP);
            // but still draw the applets
            // the popup will linger when moving onto the Config Dummy
            break;

          case INPUT_SETTINGS:
            DrawInputMappings();
            draw_applets = false;
            break;

          case QUANTIZER_SETTINGS:
            DrawQuantizerConfig();
            draw_applets = false;
            break;

          case CONFIG_SETTINGS:
            DrawConfigMenu();
            draw_applets = false;
            break;

          }
        }
        if (HS::q_edit)
          PokePopup(QUANTIZER_POPUP);

        if (draw_applets) {
          if (view_state == AUDIO_SETUP) {
            audio_app.View();

            // gfxHeader("Audio DSP Setup");
            // OC::AudioDSP::DrawAudioSetup();
            draw_applets = false;
          }
          else if (view_state == CLOCK_SETUP) {
            ClockSetup_instance.View();
            draw_applets = false;
          }
        }

        if (draw_applets) {
          if (view_state == APPLET_FULLSCREEN) {
            active_applet[zoom_slot]->BaseView(true);
            // Applets 3 and 4 get inverted titles
            if (zoom_slot > 1) gfxInvert(1 + (zoom_slot%2)*64, 1, 54, 10);
          } else {
            // only two applets visible at a time
            for (int h = 0; h < 2; h++)
            {
                HEM_SIDE slot = HEM_SIDE(h + view_slot[h]*2);
                active_applet[slot]->BaseView();

                // Applets 3 and 4 get inverted titles
                if (slot > 1) gfxInvert(1 + h*64, 1, 54, 10);
            }

            // vertical separator
            graphics.drawLine(63, 0, 63, 63, 2);
          }

          if (select_mode) {
            // screen border while X or Y is held, so they feel powerful (because they are)
            graphics.drawFrame(0, 0, 128, 64);
          }

          // Clock indicator icons in header
          if (HS::clock_m.IsRunning()) {
              gfxIcon(56, 1, HS::clock_m.Cycle() ? METRO_L_ICON : METRO_R_ICON);
          } else if (HS::clock_m.IsPaused()) {
              gfxIcon(56, 1, PAUSE_ICON);
          }
        }

        // Overlay popup window last
        if (OC::CORE::ticks - HS::popup_tick < HEMISPHERE_CURSOR_TICKS * 2) {
          HS::DrawPopup(config_cursor, preset_id, CursorBlink());
        }
    }

    // always act-on-press for encoder
    void DelegateEncoderPush(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_BUTTON_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);

        if (config_page > HIDE_CONFIG || preset_cursor) {
          ConfigButtonPush(h);
          return;
        }
        if (view_state == AUDIO_SETUP) {
          // OC::AudioDSP::AudioSetupButtonAction(h);
          audio_app.HandleButtonEvent(event);
          return;
        }

        if (view_state == CLOCK_SETUP) {
          ClockSetup_instance.OnButtonPress();
          return;
        }

        active_applet[slot]->OnButtonPress();
    }

    const HEM_SIDE ButtonToSlot(const UI::Event &event) {
        switch (event.control) {
        default:
        case OC::CONTROL_BUTTON_A:
          return LEFT_HEMISPHERE;
          break;
        case OC::CONTROL_BUTTON_X:
          return LEFT2_HEMISPHERE;
          break;
        case OC::CONTROL_BUTTON_B:
          return RIGHT_HEMISPHERE;
          break;
        case OC::CONTROL_BUTTON_Y:
          return RIGHT2_HEMISPHERE;
          break;
        }
    }

    // returns true if combo detected and button release should be ignored
    bool CheckButtonCombos(const UI::Event &event) {
        HEM_SIDE slot = ButtonToSlot(event);

        // dual press A+B for Clock Setup
        if (event.mask == (OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_B)) {
            view_state = CLOCK_SETUP;
            return true;
        }
        // dual press X+Y for Audio Setup
        if (event.mask == (OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
            view_state = AUDIO_SETUP;
            return true;
        }
        // dual press A+X for Load Preset
        if (event.mask == (OC::CONTROL_BUTTON_A | OC::CONTROL_BUTTON_X)) {
            ShowPresetSelector();
            return true;
        }

        // dual press B+Y for Input Mapping
        if (event.mask == (OC::CONTROL_BUTTON_B | OC::CONTROL_BUTTON_Y)) {
            config_page = INPUT_SETTINGS;
            config_cursor = TRIGMAP1;
            return true;
        }

        // cancel preset select or config screens
        if (config_page || preset_cursor) {
          preset_cursor = 0;
          config_page = HIDE_CONFIG;
          HS::popup_tick = 0;
          return true;
        }

        if (event.mask & (OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
          if (AUDIO_SETUP == view_state) {
            const int h = (OC::CONTROL_BUTTON_Y == event.control);
            if (OC::AudioDSP::isEditing[h]) {
              OC::AudioDSP::AudioSetupAuxButton(h);
              return true;
            }
          }
        }

        // cancel other view layers
        if (view_state != APPLETS && view_state != APPLET_FULLSCREEN) {
          view_state = APPLETS;
          return true;
        }

        // A/B/X/Y buttons becomes aux button while editing a param
        if (SlotIsVisible(slot) && active_applet[slot]->EditMode()) {
          active_applet[slot]->AuxButton();
          return true;
        }

        return false;
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);
        if (HS::q_edit) {
          HS::QEditEncoderMove(h, event.value);
          return;
        }

        if (config_page > HIDE_CONFIG || preset_cursor) {
            ConfigEncoderAction(h, event.value);
            return;
        }
        if (view_state == AUDIO_SETUP) {
          // OC::AudioDSP::AudioMenuAdjust(h, event.value);
          audio_app.HandleEncoderEvent(event);
          return;
        }

        if (view_state == CLOCK_SETUP) {
          if (h == LEFT_HEMISPHERE)
            ClockSetup_instance.OnLeftEncoderMove(event.value);
          else
            ClockSetup_instance.OnEncoderMove(event.value);
        } else if (event.mask & (OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y)) {
            // hold down X or Y to change applet with encoder
            if (view_state == APPLET_FULLSCREEN) slot = zoom_slot;
            ChangeApplet(slot, event.value);
        } else {
            active_applet[slot]->OnEncoderMove(event.value);
        }
    }

    void SetConfigPageFromCursor() {
      if (config_cursor < CONFIG_DUMMY) config_page = LOADSAVE_POPUP;
      else if (config_cursor < TRIGMAP1) config_page = CONFIG_SETTINGS;
      else if (config_cursor < QUANT1) config_page = INPUT_SETTINGS;
      else if (config_cursor < SHOWHIDELIST) config_page = QUANTIZER_SETTINGS;
    }
    void ToggleConfigMenu() {
      if (config_page) {
        config_page = HIDE_CONFIG;
      } else {
        SetConfigPageFromCursor();
      }
    }
    void ShowPresetSelector() {
      config_cursor = LOAD_PRESET;
      preset_cursor = preset_id + 1;
    }

    // this toggles the view on a given side
    void SwapViewSlot(int h) {
      // h should be 0 or 1 (left or right)
      //h %= 2;

      view_slot[h] = 1 - view_slot[h];
      // also switch fullscreen to corresponding side/slot
      zoom_slot = HEM_SIDE(view_slot[h]*2 + h);
    }

    bool SlotIsVisible(HEM_SIDE h) {
      if (view_state == APPLET_FULLSCREEN)
        return zoom_slot == h;

      return (view_slot[h % 2] == h / 2);
    }
    // this brings a specific applet into view on the appropriate side
    void SwitchToSlot(HEM_SIDE h) {
      if (view_slot[h % 2] != h / 2) {
        view_slot[h % 2] = h / 2;
      }
      zoom_slot = h;
    }

    void SetFullScreen(HEM_SIDE hemisphere) {
      zoom_slot = hemisphere;
      view_state = APPLET_FULLSCREEN;
    }
    void ToggleFullScreen() {
      view_state = (view_state == APPLET_FULLSCREEN) ? APPLETS : APPLET_FULLSCREEN;
    }

    void HandleButtonEvent(const UI::Event &event) {
        // tracks whether X or Y are being held down
        select_mode = (event.mask & (OC::CONTROL_BUTTON_X | OC::CONTROL_BUTTON_Y));

        if ((event.control == OC::CONTROL_BUTTON_L || event.control== OC::CONTROL_BUTTON_R)
            && view_state == AUDIO_SETUP) {
          // OC::AudioDSP::AudioSetupButtonAction(h);
          audio_app.HandleEncoderButtonEvent(event);
          return;
        }

        switch (event.type) {
        case UI::EVENT_BUTTON_DOWN:

          // Quantizer popup editor intercepts everything on-press
          if (HS::q_edit) {
            if (event.control == OC::CONTROL_BUTTON_UP)
              HS::NudgeOctave(HS::qview, 1);
            else if (event.control == OC::CONTROL_BUTTON_DOWN)
              HS::NudgeOctave(HS::qview, -1);
            else {
              HS::q_edit = false;
              select_mode = false;
            }

            OC::ui.SetButtonIgnoreMask();
            break;
          }

          switch (event.control) {
            case OC::CONTROL_BUTTON_Z:
              // X or Y + Z == go fullscreen
              if (select_mode) {
                bool h = (event.mask & OC::CONTROL_BUTTON_Y); // left or right
                zoom_slot = HEM_SIDE(view_slot[h]*2 + h);
                ToggleFullScreen();
                select_mode = false;
                OC::ui.SetButtonIgnoreMask();
                break;
              }

              // Z-button - Zap the CLOCK!!
              ToggleClockRun();
              OC::ui.SetButtonIgnoreMask();
              break;

            case OC::CONTROL_BUTTON_L:
            case OC::CONTROL_BUTTON_R:
              if (event.mask == (OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R)) {
                // TODO: how to go to app menu?
              }
              DelegateEncoderPush(event);
              // ignore long-press to prevent Main Menu >:)
              //OC::ui.SetButtonIgnoreMask();
              break;

            case OC::CONTROL_BUTTON_A:
            case OC::CONTROL_BUTTON_B:
            case OC::CONTROL_BUTTON_X:
            case OC::CONTROL_BUTTON_Y:
              if (CheckButtonCombos(event)) {
                select_mode = false;
                OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
              }
              break;

            default:
              break;
          }

          break;

        case UI::EVENT_BUTTON_PRESS:
          switch (event.control) {
            // A/B/X/Y switch to corresponding applet on release
            case OC::CONTROL_BUTTON_A:
            case OC::CONTROL_BUTTON_B:
            case OC::CONTROL_BUTTON_X:
            case OC::CONTROL_BUTTON_Y:
            {
              HEM_SIDE slot = ButtonToSlot(event);
              if (view_state == APPLET_FULLSCREEN && slot == zoom_slot)
                view_state = APPLETS;

              SwitchToSlot(slot);
              break;
            }

            // ignore all other button release events
            default: break;
          }
          break;

        case UI::EVENT_BUTTON_LONG_PRESS:
          if (event.control == OC::CONTROL_BUTTON_B) ToggleConfigMenu();
          break;

        default: break;
        }
    }

private:
    int preset_id = 0;
    int queued_preset = 0;
    int preset_cursor = 0;
    HemisphereApplet *active_applet[4]; // Pointers to actual applets
    int active_applet_index[4]; // Indexes to available_applets
                      // Left side: 0,2
                      // Right side: 1,3
    int next_applet_index[4]; // queued from UI thread, handled by Controller
    uint64_t clock_data, global_data, applet_data[4]; // cache of applet data
    bool view_slot[2] = {0, 0}; // Two applets on each side, only one visible at a time
    int config_cursor = 0;

    bool select_mode = 0;
    HEM_SIDE zoom_slot; // Which of the hemispheres (if any) is in fullscreen/help mode

    // State machine
    enum QuadrantsView {
      APPLETS,
      APPLET_FULLSCREEN,
      //CONFIG_MENU,
      //PRESET_PICKER,
      CLOCK_SETUP,
      AUDIO_SETUP,
    };
    QuadrantsView view_state = APPLETS;

    enum QuadrantsConfigPage {
      HIDE_CONFIG,
      LOADSAVE_POPUP,
      CONFIG_SETTINGS,
      INPUT_SETTINGS,
      QUANTIZER_SETTINGS,
      SHOWHIDE_APPLETS,

      LAST_PAGE = SHOWHIDE_APPLETS
    };
    int config_page = HIDE_CONFIG;

    enum QuadrantsConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,

        // Input Remapping
        TRIGMAP1, TRIGMAP2, TRIGMAP3, TRIGMAP4,
        CVMAP1, CVMAP2, CVMAP3, CVMAP4,
        TRIGMAP5, TRIGMAP6, TRIGMAP7, TRIGMAP8,
        CVMAP5, CVMAP6, CVMAP7, CVMAP8,

        // Global Quantizers: 4x(Scale, Root, Octave, Mask?)
        QUANT1, QUANT2, QUANT3, QUANT4,
        QUANT5, QUANT6, QUANT7, QUANT8,

        // Applet visibility (dummy position)
        SHOWHIDELIST,

        MAX_CURSOR = QUANT8
    };

    void ConfigEncoderAction(int h, int dir) {
        if (!isEditing && !preset_cursor) {
          if (h == 0) { // change pages
            config_page += dir;
            config_page = constrain(config_page, LOADSAVE_POPUP, QUANTIZER_SETTINGS);

            const int cursorpos[] = { 0, LOAD_PRESET, TRIG_LENGTH, TRIGMAP1, QUANT1, SHOWHIDELIST };
            config_cursor = cursorpos[config_page];
          } else { // move cursor
            config_cursor += dir;
            config_cursor = constrain(config_cursor, 0, MAX_CURSOR);

            SetConfigPageFromCursor();
          }
          ResetCursor();
          return;
        }

        switch (config_cursor) {
        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
            HS::trigger_mapping[config_cursor-TRIGMAP1] = constrain(
                HS::trigger_mapping[config_cursor-TRIGMAP1] + dir, 0, TRIGMAP_MAX);
            break;
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
            HS::cvmapping[config_cursor-CVMAP1] =
              constrain( HS::cvmapping[config_cursor-CVMAP1] + dir, 0, CVMAP_MAX);
            break;
        case TRIGMAP5:
        case TRIGMAP6:
        case TRIGMAP7:
        case TRIGMAP8:
            HS::trigger_mapping[config_cursor-TRIGMAP5 + 4] = constrain(
                HS::trigger_mapping[config_cursor-TRIGMAP5 + 4] + dir, 0, TRIGMAP_MAX);
            break;
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
            HS::cvmapping[config_cursor-CVMAP5 + 4] =
              constrain( HS::cvmapping[config_cursor-CVMAP5 + 4] + dir, 0, CVMAP_MAX);
            break;
        case TRIG_LENGTH:
            HS::trig_length = (uint32_t) constrain( int(HS::trig_length + dir), 1, 127);
            break;
        //case SCREENSAVER_MODE:
            // TODO?
            //break;
        case LOAD_PRESET:
        case SAVE_PRESET:
            if (h == 0) {
              config_cursor = constrain(config_cursor + dir, LOAD_PRESET, SAVE_PRESET);
            } else {
              preset_cursor = constrain(preset_cursor + dir, 1, QUAD_PRESET_COUNT);
            }
            break;
        }
    }
    void ConfigButtonPush(int h) {
        if (preset_cursor) {
            // Save or Load on button push
            if (config_cursor == SAVE_PRESET)
                StoreToPreset(preset_cursor-1);
            else {
                QueuePresetLoad(preset_cursor - 1);
            }

            preset_cursor = 0; // deactivate preset selection
            config_page = HIDE_CONFIG;
            view_state = APPLETS;
            isEditing = false;
            return;
        }

        switch (config_cursor) {
        case SAVE_PRESET:
        case LOAD_PRESET:
            preset_cursor = preset_id + 1;
            break;

        case AUTO_SAVE:
            HS::auto_save_enabled = !HS::auto_save_enabled;
            break;

        case QUANT1:
        case QUANT2:
        case QUANT3:
        case QUANT4:
        case QUANT5:
        case QUANT6:
        case QUANT7:
        case QUANT8:
            HS::QuantizerEdit(config_cursor - QUANT1);
            break;

        case TRIGMAP1:
        case TRIGMAP2:
        case TRIGMAP3:
        case TRIGMAP4:
        case TRIGMAP5:
        case TRIGMAP6:
        case TRIGMAP7:
        case TRIGMAP8:
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
        case TRIG_LENGTH:
            isEditing = !isEditing;
            break;

        case SCREENSAVER_MODE:
            ++HS::screensaver_mode %= 4;
            break;

        case CURSOR_MODE:
            HS::cursor_wrap = !HS::cursor_wrap;
            break;

        default: break;
        }
    }

    void DrawInputMappings() {
        gfxHeader("<  Input Mapping  >");
        gfxIcon(25, 13, TR_ICON); gfxIcon(89, 13, TR_ICON);
        gfxIcon(25, 26, CV_ICON); gfxIcon(89, 26, CV_ICON);
        gfxIcon(25, 39, TR_ICON); gfxIcon(89, 39, TR_ICON);
        gfxIcon(25, 52, CV_ICON); gfxIcon(89, 52, CV_ICON);

        for (int ch=0; ch<4; ++ch) {
          // Physical trigger input mappings
          // Physical CV input mappings
          // Top 2 applets
          gfxPrint(4 + ch*32, 15, OC::Strings::trigger_input_names_none[ HS::trigger_mapping[ch] ] );
          gfxPrint(4 + ch*32, 28, OC::Strings::cv_input_names_none[ HS::cvmapping[ch] ] );

          // Bottom 2 applets
          gfxPrint(4 + ch*32, 41, OC::Strings::trigger_input_names_none[ HS::trigger_mapping[ch + 4] ] );
          gfxPrint(4 + ch*32, 54, OC::Strings::cv_input_names_none[ HS::cvmapping[ch + 4] ] );
        }

        gfxDottedLine(64, 11, 64, 63); // vert
        gfxDottedLine(0, 38, 127, 38); // horiz

        // Cursor location is within a 4x4 grid
        const int cur_x = (config_cursor-TRIGMAP1) % 4;
        const int cur_y = (config_cursor-TRIGMAP1) / 4;

        gfxCursor(4 + 32*cur_x, 23 + 13*cur_y, 19);

    }
    void DrawQuantizerConfig() {
        gfxHeader("<  Quantizer Setup");

        for (int ch=0; ch<4; ++ch) {
          const int x = 8 + ch*32;

          // 1-4 on top
          gfxPrint(x, 15, "Q");
          gfxPrint(ch + 1);
          gfxLine(x, 23, x + 14, 23);
          gfxLine(x + 14, 13, x + 14, 23);

          const bool upper = config_cursor < QUANT5;
          const int ch_view = upper ? ch : ch + 4;

          gfxIcon(x + 3, upper? 25 : 45, upper? UP_BTN_ICON : DOWN_BTN_ICON);

          // Scale
          gfxPrint(x - 3, 30, OC::scale_names_short[ HS::quant_scale[ch_view] ]);

          // Root Note + Octave
          gfxPrint(x - 3, 40, OC::Strings::note_names[ HS::root_note[ch_view] ]);
          if (HS::q_octave[ch_view] >= 0) gfxPrint("+");
          gfxPrint(HS::q_octave[ch_view]);

          // (TODO: mask editor)

          // 5-8 on bottom
          gfxPrint(x, 55, "Q");
          gfxPrint(ch + 5);
          gfxLine(x, 53, x + 14, 53);
          gfxLine(x + 14, 53, x + 14, 63);
        }

        switch (config_cursor) {
        case QUANT1:
        case QUANT2:
        case QUANT3:
        case QUANT4:
          gfxIcon( 32*(config_cursor-QUANT1), 15, RIGHT_ICON);
          break;
        case QUANT5:
        case QUANT6:
        case QUANT7:
        case QUANT8:
          gfxIcon( 32*(config_cursor-QUANT5), 55, RIGHT_ICON);
          break;
        }
    }

    void DrawConfigMenu() {
        // --- Config Selection
        gfxHeader("< General Settings  >");

        gfxPrint(1, 15, "Trig Length: ");
        gfxPrint(HS::trig_length);
        gfxPrint("ms");

        const char * ssmodes[4] = { "[blank]", "Meters", "Zaps",
        #if defined(__IMXRT1062__)
        "Stars"
        #else
        "Zips"
        #endif
        };
        gfxPrint(1, 25, "Screensaver:  ");
        gfxPrint( ssmodes[HS::screensaver_mode] );

        const char * cursor_mode_name[3] = { "modal", "modal+wrap" };
        gfxPrint(1, 35, "Cursor:  ");
        gfxPrint(cursor_mode_name[HS::cursor_wrap]);
        
        switch (config_cursor) {
        case TRIG_LENGTH:
            gfxCursor(80, 23, 24);
            break;
        case SCREENSAVER_MODE:
            gfxIcon(73, 25, RIGHT_ICON);
            break;
        case CURSOR_MODE:
            gfxIcon(43, 35, RIGHT_ICON);
            break;
        case CONFIG_DUMMY:
            gfxIcon(2, 1, LEFT_ICON);
            break;
        }
    }

    void DrawPresetSelector() {
        gfxHeader((config_cursor == SAVE_PRESET) ? "Save" : "Load");
        gfxPrint(30, 1, "Preset");
        gfxDottedLine(16, 11, 16, 63);

        int y = 5 + constrain(preset_cursor,1,5)*10;
        gfxIcon(0, y, RIGHT_ICON);
        const int top = constrain(preset_cursor - 4, 1, QUAD_PRESET_COUNT) - 1;
        y = 15;
        for (int i = top; i < QUAD_PRESET_COUNT && i < top + 5; ++i)
        {
            if (i == preset_id)
              gfxIcon(8, y, ZAP_ICON);
            else
              gfxPrint(8, y, OC::Strings::capital_letters[i]);

            if (!quad_presets[i].is_valid())
                gfxPrint(18, y, "(empty)");
            else {
                gfxPrint(18, y, quad_presets[i].GetApplet(LEFT_HEMISPHERE)->applet_name());
                gfxPrint(", ");
                gfxPrint(quad_presets[i].GetApplet(RIGHT_HEMISPHERE)->applet_name());
            }

            y += 10;
        }
    }
};

// TOTAL EEPROM SIZE: 4 * 56 bytes
SETTINGS_DECLARE(QuadrantsPreset, QUADRANTS_SETTING_LAST) {
    {0, 0, 255, "Applet ID L", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Applet ID R", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Applet ID L2", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 255, "Applet ID R2", NULL, settings::STORAGE_TYPE_U8},
    {0, 0, 65535, "Data L block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L2 block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L2 block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L2 block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data L2 block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R2 block 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R2 block 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R2 block 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Data R2 block 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Clock data 4", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Trig Map 123", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Trig Map 456", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Trig Map 78",  NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Input Map1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Input Map2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Input Map3", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Globals1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Globals2", NULL, settings::STORAGE_TYPE_U16}
};

QuadAppletManager quad_manager;

void QuadrantSysExHandler() {
    if (quad_active_preset)
        quad_active_preset->OnReceiveSysEx();
}
void QuadrantBeatSync() {
  quad_manager.ProcessQueue();
}

////////////////////////////////////////////////////////////////////////////////
//// O_C App Functions
////////////////////////////////////////////////////////////////////////////////

// App stubs
void QUADRANTS_init() {
    quad_manager.BaseStart();
}

static constexpr size_t QUADRANTS_storageSize() {
    return QuadrantsPreset::storageSize() * QUAD_PRESET_COUNT;
}

static size_t QUADRANTS_save(void *storage) {
    size_t used = 0;
    for (int i = 0; i < QUAD_PRESET_COUNT; ++i) {
        used += quad_presets[i].Save(static_cast<char*>(storage) + used);
    }
    return used;
}

static size_t QUADRANTS_restore(const void *storage) {
    size_t used = 0;
    for (int i = 0; i < QUAD_PRESET_COUNT; ++i) {
        used += quad_presets[i].Restore(static_cast<const char*>(storage) + used);
    }
    return used;
}

void FASTRUN QUADRANTS_isr() {
    quad_manager.BaseController();
}

void QUADRANTS_handleAppEvent(OC::AppEvent event) {
    switch (event) {
    case OC::APP_EVENT_RESUME:
        quad_manager.Resume();
        break;

    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SUSPEND:
        quad_manager.Suspend();
        break;

    default: break;
    }
}

void QUADRANTS_loop() {} // Essentially deprecated in favor of ISR

void QUADRANTS_menu() {
    quad_manager.View();
}

void QUADRANTS_screensaver() {
    switch (HS::screensaver_mode) {
    case 0x3: // Zips or Stars
        ZapScreensaver(true);
        break;
    case 0x2: // Zaps
        ZapScreensaver();
        break;
    case 0x1: // Meters
        quad_manager.BaseScreensaver(true); // show note names
        break;
    default: break; // blank screen
    }
}

void QUADRANTS_handleButtonEvent(const UI::Event &event) {
    quad_manager.HandleButtonEvent(event);
}

void QUADRANTS_handleEncoderEvent(const UI::Event &event) {
    quad_manager.DelegateEncoderMovement(event);
}
