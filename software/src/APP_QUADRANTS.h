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

#include "hemisphere_config.h"

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
    QUADRANTS_TRIGMAP1,
    QUADRANTS_TRIGMAP2,
    QUADRANTS_CVMAP1,
    QUADRANTS_CVMAP2,
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
      uint32_t trigmap = 0;
      for (size_t i = 0; i < 8; ++i) {
        trigmap |= (uint32_t(HS::trigger_mapping[i] + 1) & 0x0F) << (i*4);
      }

      bool changed = (trigmap != ( uint32_t(values_[QUADRANTS_TRIGMAP1])
                                | (uint32_t(values_[QUADRANTS_TRIGMAP2]) << 16) ));
      values_[QUADRANTS_TRIGMAP1] = trigmap & 0xFFFF;
      values_[QUADRANTS_TRIGMAP2] = (trigmap >> 16) & 0xFFFF;
      return changed;
    }
    void LoadInputMap() {
      // TODO: cvmap
      for (size_t i = 0; i < 4; ++i) {
        int val1 = (uint32_t(values_[QUADRANTS_TRIGMAP1]) >> (i*4)) & 0x0F;
        int val2 = (uint32_t(values_[QUADRANTS_TRIGMAP2]) >> (i*4)) & 0x0F;
        if (val1 != 0) HS::trigger_mapping[i] = constrain(val1 - 1, 0, 8);
        if (val2 != 0) HS::trigger_mapping[i+4] = constrain(val2 - 1, 0, 8);
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

class QuadAppletManager : public HSApplication {
public:
    enum PopupType {
      MENU_POPUP,
      CLOCK_POPUP, PRESET_POPUP,
    };
    void Start() {
        //select_mode = -1; // Not selecting

        help_hemisphere = -1;
        clock_setup = 0;

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
            int index = my_applet[h];
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
            OC::CORE::app_isr_enabled = false;
            OC::draw_save_message(60);
            delay(1);
            OC::save_app_data();
            delay(1);
            OC::CORE::app_isr_enabled = true;
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

    // does not modify the preset, only the quad_manager
    void SetApplet(HEM_SIDE hemisphere, int index) {
        next_applet[hemisphere] = my_applet[hemisphere] = index;
        HS::available_applets[index].instance[hemisphere]->BaseStart(hemisphere);
    }
    void ChangeApplet(HEM_SIDE h, int dir) {
        int index = HS::get_next_applet_index(next_applet[h], dir);
        next_applet[h] = index;
    }

    bool SelectModeEnabled() {
        return select_mode > -1;
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
                if (slot < QUAD_PRESET_COUNT) LoadFromPreset(slot);
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
            if (my_applet[h] != next_applet[h]) {
              SetApplet(HEM_SIDE(h), next_applet[h]);
            }
            int index = my_applet[h];

            // MIDI signals mixed with inputs to applets
            if (HS::available_applets[index].id != 150) // not MIDI In
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
            HS::available_applets[index].instance[h]->BaseController();
        }
    }

    inline void PokePopup(PopupType pop) {
      popup_type = pop;
      popup_tick = OC::CORE::ticks;
    }

    void DrawPopup() {
      if (popup_type == MENU_POPUP) {
        graphics.clearRect(73, 25, 54, 38);
        graphics.drawFrame(74, 26, 52, 36);
      } else {
        graphics.clearRect(24, 23, 80, 18);
        graphics.drawFrame(25, 24, 78, 16);
        graphics.setPrintPos(29, 28);
      }

      switch (popup_type) {
        case MENU_POPUP:
          gfxPrint(78, 30, "Load");
          gfxPrint(78, 40, config_cursor == AUTO_SAVE ? "(auto)" : "Save");
          gfxPrint(78, 50, "Config");

          switch (config_cursor) {
            case LOAD_PRESET:
            case SAVE_PRESET:
              gfxIcon(104, 30 + (config_cursor-LOAD_PRESET)*10, LEFT_ICON);
              break;
            case AUTO_SAVE:
              if (CursorBlink())
                gfxIcon(114, 40, HS::auto_save_enabled ? CHECK_ON_ICON : CHECK_OFF_ICON );
              break;
            case CONFIG_DUMMY:
              gfxIcon(115, 50, RIGHT_ICON);
              break;
            default: break;
          }

          break;
        case CLOCK_POPUP:
          graphics.print("Clock ");
          if (HS::clock_m.IsRunning())
            graphics.print("Start");
          else
            graphics.print(HS::clock_m.IsPaused() ? "Armed" : "Stop");
          break;

        case PRESET_POPUP:
          graphics.print("> Preset ");
          graphics.print(OC::Strings::capital_letters[preset_id]);
          break;
      }
    }

    void View() {
        bool draw_applets = true;

        if (preset_cursor) {
          DrawPresetSelector();
          draw_applets = false;
        }
        else if (config_menu) {
          if (config_cursor < CONFIG_DUMMY) {
            PokePopup(MENU_POPUP);
            // but still draw the applets
          } else {
            // the popup will linger when moving onto the Config Dummy
            DrawConfigMenu();
            draw_applets = false;
          }

          // if (!draw_applets && popup_type == MENU_POPUP) popup_tick = 0; // cancel popup
        }

        if (draw_applets) {
          if (clock_setup) {
            ClockSetup_instance.View();
            draw_applets = false;
          }
          else if (help_hemisphere > -1) {
            int index = my_applet[help_hemisphere];
            HS::available_applets[index].instance[help_hemisphere]->BaseView();
            draw_applets = false;
          }
        }

        if (draw_applets) {
            if (HS::clock_m.IsRunning()) {
                // Metronome icon
                gfxIcon(56, 1, HS::clock_m.Cycle() ? METRO_L_ICON : METRO_R_ICON);
            } else if (HS::clock_m.IsPaused()) {
                gfxIcon(56, 1, PAUSE_ICON);
            }

            // only two applets visible at a time
            for (int h = 0; h < 2; h++)
            {
                HEM_SIDE slot = HEM_SIDE(h + view_slot[h]*2);
                int index = my_applet[slot];
                HS::available_applets[index].instance[slot]->BaseView();

                // Applets 3 and 4 get inverted titles
                if (slot > 1) gfxInvert(1 + h*64, 1, 54, 10);
            }

            if (select_mode % 2 == LEFT_HEMISPHERE) graphics.drawFrame(0, 0, 64, 64);
            if (select_mode % 2 == RIGHT_HEMISPHERE) graphics.drawFrame(64, 0, 64, 64);
        }

        // Overlay popup window last
        if (OC::CORE::ticks - popup_tick < HEMISPHERE_CURSOR_TICKS) {
            DrawPopup();
        }
    }

    void DelegateEncoderPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        int h = (event.control == OC::CONTROL_BUTTON_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);

        if (config_menu || preset_cursor) {
            // button release for config screen
            if (!down) ConfigButtonPush(h);
            return;
        }

        // button down
        if (down) {
            // Clock Setup is more immediate for manual triggers
            if (clock_setup) ClockSetup_instance.OnButtonPress();
            // TODO: consider a new OnButtonDown handler for applets
            return;
        }

        // button release
        if (select_mode == slot) {
            select_mode = -1; // Pushing a button for the selected side turns off select mode
        } else if (!clock_setup) {
            // regular applets get button release
            int index = my_applet[slot];
            HS::available_applets[index].instance[slot]->OnButtonPress();
        }
    }

    /*
    void ExtraButtonPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        if (down) return;

        if (preset_cursor) {
            preset_cursor = 0;
            return;
        }
        if (config_menu) {
            // cancel preset select, or config screen on select button release
            config_menu = 0;
            popup_tick = 0;
            return;
        }

        if (clock_setup) {
            clock_setup = 0; // Turn off clock setup with any single-click button release
            return;
        }

        if (event.control == OC::CONTROL_BUTTON_DOWN2)
            ToggleConfigMenu();

        if (event.control == OC::CONTROL_BUTTON_UP2)
            ShowPresetSelector();

    }
    */
    const HEM_SIDE ButtonToSlot(const UI::Event &event) {
        switch (event.control) {
        default:
        case OC::CONTROL_BUTTON_UP:
          return LEFT_HEMISPHERE;
          break;
        case OC::CONTROL_BUTTON_DOWN:
          return RIGHT_HEMISPHERE;
          break;
        case OC::CONTROL_BUTTON_UP2:
          return LEFT2_HEMISPHERE;
          break;
        case OC::CONTROL_BUTTON_DOWN2:
          return RIGHT2_HEMISPHERE;
          break;
        }
    }

    void DelegateSelectButtonPush(const UI::Event &event) {
        bool down = (event.type == UI::EVENT_BUTTON_DOWN);
        HEM_SIDE hemisphere = ButtonToSlot(event);

        if (preset_cursor && !down) {
            preset_cursor = 0;
            return;
        }
        if (config_menu && !down) {
            // cancel preset select, or config screen on select button release
            config_menu = 0;
            popup_tick = 0;
            return;
        }

        if (clock_setup && !down) {
            clock_setup = 0; // Turn off clock setup with any single-click button release
            return;
        }

        // -- button down
        if (down) {
            // dual press for Clock Setup... check first_click, so we only process the 2nd button event
            if (event.mask == (OC::CONTROL_BUTTON_UP | OC::CONTROL_BUTTON_DOWN) && hemisphere != first_click) {
                clock_setup = 1;
                SetHelpScreen(-1);
                select_mode = -1;
                OC::ui.SetButtonIgnoreMask(); // ignore button release
                return;
            }

            if (OC::CORE::ticks - click_tick < HEMISPHERE_DOUBLE_CLICK_TIME && hemisphere == first_click) {
                // This is a double-click on one button. Activate corresponding help screen and deactivate select mode.
                SetHelpScreen(hemisphere);

                // reset double-click timer either way
                click_tick = 0;
                return;
            }

            // -- Single click
            // If a help screen is already selected, and the button is for
            // the opposite one, go to the other help screen
            if (help_hemisphere > -1) {
                if (help_hemisphere != hemisphere) SetHelpScreen(hemisphere);
                else SetHelpScreen(-1); // Exit help screen if same button is clicked
                OC::ui.SetButtonIgnoreMask(); // ignore release
            }
            // mark this single click
            click_tick = OC::CORE::ticks;
            first_click = hemisphere;
            return;
        }

        // -- button release
        if (!clock_setup) {
            const int index = my_applet[hemisphere];
            HemisphereApplet* applet = HS::available_applets[index].instance[hemisphere];

            // switching views
            if (view_slot[hemisphere % 2] != hemisphere / 2) {
              view_slot[hemisphere % 2] = hemisphere / 2;
              select_mode = -1;
            } else if (applet->EditMode()) {
              // select button becomes aux button while editing a param
              applet->AuxButton();
            } else {
              // Select Mode
              if (hemisphere == select_mode) select_mode = -1; // Exit Select Mode if same button is pressed
              else if (help_hemisphere < 0) // Otherwise, set Select Mode - UNLESS there's a help screen
                  select_mode = hemisphere;
            }
        }
    }

    void DelegateEncoderMovement(const UI::Event &event) {
        int h = (event.control == OC::CONTROL_ENCODER_L) ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE;
        HEM_SIDE slot = HEM_SIDE(view_slot[h]*2 + h);
        if (config_menu || preset_cursor) {
            ConfigEncoderAction(h, event.value);
            return;
        }

        if (clock_setup) {
            ClockSetup_instance.OnEncoderMove(event.value);
        } else if (select_mode == slot) {
            ChangeApplet(slot, event.value);
        } else {
            int index = my_applet[slot];
            HS::available_applets[index].instance[slot]->OnEncoderMove(event.value);
        }
    }

    void ToggleClockRun() {
        if (HS::clock_m.IsRunning()) {
            HS::clock_m.Stop();
        } else {
            bool p = HS::clock_m.IsPaused();
            HS::clock_m.Start( !p );
        }
        PokePopup(CLOCK_POPUP);
    }

    void ToggleClockSetup() {
        clock_setup = 1 - clock_setup;
    }

    void ToggleConfigMenu() {
        config_menu = !config_menu;
        if (config_menu) SetHelpScreen(-1);
    }
    void ShowPresetSelector() {
        config_cursor = LOAD_PRESET;
        preset_cursor = preset_id + 1;
    }

    void SetHelpScreen(int hemisphere) {
        if (help_hemisphere > -1) { // Turn off the previous help screen
            int index = my_applet[help_hemisphere];
            HS::available_applets[index].instance[help_hemisphere]->ToggleHelpScreen();
        }

        if (hemisphere > -1) { // Turn on the next hemisphere's screen
            int index = my_applet[hemisphere];
            HS::available_applets[index].instance[hemisphere]->ToggleHelpScreen();
        }

        help_hemisphere = hemisphere;
    }

    void HandleButtonEvent(const UI::Event &event) {
        switch (event.type) {
        case UI::EVENT_BUTTON_DOWN:
            if (event.control == OC::CONTROL_BUTTON_M) {
                ToggleClockRun();
                OC::ui.SetButtonIgnoreMask(); // ignore release and long-press
                break;
            }
        case UI::EVENT_BUTTON_PRESS:
            if (event.control == OC::CONTROL_BUTTON_L || event.control == OC::CONTROL_BUTTON_R) {
                DelegateEncoderPush(event);
            } else
                DelegateSelectButtonPush(event);

            break;

        case UI::EVENT_BUTTON_LONG_PRESS:
            if (event.control == OC::CONTROL_BUTTON_DOWN) ToggleConfigMenu();
            if (event.control == OC::CONTROL_BUTTON_L) ToggleClockRun();
            break;

        default: break;
        }
    }

private:
    int preset_id = 0;
    int preset_cursor = 0;
    int my_applet[4]; // Indexes to available_applets
                      // Left side: 0,2
                      // Right side: 1,3
    int next_applet[4]; // queued from UI thread, handled by Controller
    uint64_t clock_data, global_data, applet_data[4]; // cache of applet data
    bool view_slot[2] = {0, 0}; // Two applets on each side, only one visible at a time
    bool clock_setup;
    bool config_menu;
    bool isEditing = false;
    int config_cursor = 0;

    int help_hemisphere; // Which of the hemispheres (if any) is in help mode, or -1 if none
    uint32_t click_tick; // Measure time between clicks for double-click
    uint32_t popup_tick; // for button feedback
    PopupType popup_type = PRESET_POPUP;
    HEM_SIDE first_click; // The first button pushed of a double-click set, to see if the same one is pressed

    enum QuadrantsConfigCursor {
        LOAD_PRESET, SAVE_PRESET,
        AUTO_SAVE,
        CONFIG_DUMMY, // past this point goes full screen
        TRIG_LENGTH,
        SCREENSAVER_MODE,
        CURSOR_MODE,

        CVMAP1, CVMAP2, CVMAP3, CVMAP4,
        CVMAP5, CVMAP6, CVMAP7, CVMAP8,

        MAX_CURSOR = CVMAP8
    };

    void ConfigEncoderAction(int h, int dir) {
        if (!isEditing && !preset_cursor) {
            config_cursor += dir;
            config_cursor = constrain(config_cursor, 0, MAX_CURSOR);
            ResetCursor();
            return;
        }

        switch (config_cursor) {
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
            HS::cvmapping[config_cursor-CVMAP1] = constrain( HS::cvmapping[config_cursor-CVMAP1] + dir, 0, ADC_CHANNEL_LAST);
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
            else
                LoadFromPreset(preset_cursor-1);

            preset_cursor = 0; // deactivate preset selection
            config_menu = 0;
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
        }
    }

    void DrawConfigMenu() {
        // --- Config Selection
        gfxHeader("< Presets / Config");

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
        
        // Physical CV input mappings
        for (int ch=0; ch<8; ++ch) {
          const int x = (32 * ch) % 128;
          const int y = ch > 3 ? 55 : 45;
          gfxPrint(1 + x, y, OC::Strings::cv_input_names_none[ HS::cvmapping[ch] ] );
        }

        switch (config_cursor) {
        case CVMAP1:
        case CVMAP2:
        case CVMAP3:
        case CVMAP4:
        case CVMAP5:
        case CVMAP6:
        case CVMAP7:
        case CVMAP8:
        {
          const int x = 32*(config_cursor-CVMAP1) % 128;
          const int y = config_cursor > CVMAP4 ? 55 : 45;
          if (isEditing) gfxInvert(x, y-1, 20, 9);
          else gfxCursor(1+x, y+8, 19);
          break;
        }

        case TRIG_LENGTH:
            if (isEditing) gfxInvert(79, 14, 25, 9);
            else gfxCursor(80, 23, 24);
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
    {0, 0, 65535, "Trig Map 1234", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Trig Map 5678", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Input Map1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "CV Input Map2", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Globals1", NULL, settings::STORAGE_TYPE_U16},
    {0, 0, 65535, "Globals2", NULL, settings::STORAGE_TYPE_U16}
};

QuadAppletManager quad_manager;

void QuadrantSysExHandler() {
    if (quad_active_preset)
        quad_active_preset->OnReceiveSysEx();
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
