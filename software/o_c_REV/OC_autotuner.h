#ifndef OC_AUTOTUNER_H
#define OC_AUTOTUNER_H

#include "OC_autotune.h"
#include "OC_options.h"
#include "OC_visualfx.h"

// autotune constants:
#ifdef VOR
static constexpr int ACTIVE_OCTAVES = OCTAVES;
#else
static constexpr int ACTIVE_OCTAVES = OCTAVES - 1;
#endif

static constexpr size_t kHistoryDepth = 10;
  
#define FREQ_MEASURE_TIMEOUT 512
#define ERROR_TIMEOUT (FREQ_MEASURE_TIMEOUT << 0x4)
#define MAX_NUM_PASSES 1500
#define CONVERGE_PASSES 5

#if defined(BUCHLA_4U) && !defined(IO_10V)
const char* const AT_steps[] = {
  "0.0V", "1.2V", "2.4V", "3.6V", "4.8V", "6.0V", "7.2V", "8.4V", "9.6V", "10.8V", " " 
};
#elif defined(IO_10V)
const char* const AT_steps[] = {
  "0.0V", "1.0V", "2.0V", "3.0V", "4.0V", "5.0V", "6.0V", "7.0V", "8.0V", "9.0V", " " 
};
#elif defined(VOR)
const char* const AT_steps[] = {
  "0.0V", "1.0V", "2.0V", "3.0V", "4.0V", "5.0V", "6.0V", "7.0V", "8.0V", "9.0V", "10.0V", " " 
};
#else
const char* const AT_steps[] = {
  "-3V", "-2V", "-1V", " 0V", "+1V", "+2V", "+3V", "+4V", "+5V", "+6V", " " 
};
#endif

// 
#ifdef BUCHLA_4U
  constexpr float target_multipliers[OCTAVES] = { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f, 512.0f };
#else
  constexpr float target_multipliers[OCTAVES + 6] = { 0.03125f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f, 512.0f, 1024.0f };
//  constexpr float target_multipliers[OCTAVES] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f };
#endif

#ifdef BUCHLA_SUPPORT
  constexpr float target_multipliers_1V2[OCTAVES] = {
    0.1767766952966368931843f,
    0.3149802624737182976666f,
    0.5612310241546865086093f,
    1.0f,
    1.7817974362806785482150f,
    3.1748021039363991668836f,
    5.6568542494923805818985f,
    10.0793683991589855253324f,
    17.9593927729499718282113f,
    32.0f
  };

  constexpr float target_multipliers_2V0[OCTAVES] = {
    0.3535533905932737863687f,
    0.5f,
    0.7071067811865475727373f,
    1.0f,
    1.4142135623730951454746f,
    2.0f,
    2.8284271247461902909492f,
    4.0f,
    5.6568542494923805818985f,
    8.0f
  };
#endif

namespace OC {

enum AUTO_MENU_ITEMS {
  DATA_SELECT,
  AUTOTUNE,
  AUTORUN,
  AUTO_MENU_ITEMS_LAST
};

enum AT_STATUS {
   AT_OFF,
   AT_READY,
   AT_RUN,
   AT_ERROR,
   AT_DONE,
   AT_LAST,
};

enum AUTO_CALIBRATION_STEP {
  DAC_VOLT_0_ARM,
  DAC_VOLT_0_BASELINE,
  DAC_VOLT_TARGET_FREQUENCIES,
  DAC_VOLT_3m, 
  DAC_VOLT_2m, 
  DAC_VOLT_1m, 
  DAC_VOLT_0, 
  DAC_VOLT_1, 
  DAC_VOLT_2, 
  DAC_VOLT_3, 
  DAC_VOLT_4, 
  DAC_VOLT_5, 
  DAC_VOLT_6,
#ifdef VOR
  DAC_VOLT_7,
#endif
  AUTO_CALIBRATION_STEP_LAST
};

template <typename Owner>
class Autotuner {

public:

  void Init() {
    owner_ = nullptr;
    cursor_pos_ = 0;
    data_select_ = 0;
    channel_ = DAC_CHANNEL_A;
    calibration_data_ = 0;
    auto_tune_running_status_ = AT_OFF;

    // moved from REFS
    armed_ = false;
    autotuner_step_ = OC::DAC_VOLT_0_ARM;
    auto_DAC_offset_error_ = 0;
    auto_frequency_ = 0;
    auto_last_frequency_ = 0;
    auto_freq_sum_ = 0;
    auto_freq_count_ = 0;
    auto_ready_ = 0;
    ticks_since_last_freq_ = 0;
    auto_next_step_ = false;
    autotune_completed_ = false;
    F_correction_factor_ = 0xFF;
    correction_direction_ = false;
    correction_cnt_positive_ = 0x0;
    correction_cnt_negative_ = 0x0;
    reset_calibration_data();

    history_.Init(0x0);
  }

  bool active() const {
    return nullptr != owner_;
  }

  void Open(Owner *owner) {
    owner_ = owner;
    channel_ = owner_->get_channel();
    Begin();
  }

  void Reset() {
      reset_autotuner();
  }

  void ISR();
  void Close();
  void Draw();
  void HandleButtonEvent(const UI::Event &event);
  void HandleEncoderEvent(const UI::Event &event);
  
private:

  Owner *owner_;
  size_t cursor_pos_;
  size_t data_select_;
  DAC_CHANNEL channel_;
  uint8_t calibration_data_;
  AT_STATUS auto_tune_running_status_;

  // moved from References
  OC::vfx::ScrollingHistory<uint32_t, kHistoryDepth> history_;

  bool armed_;
  uint8_t autotuner_step_;
  int32_t auto_DAC_offset_error_;
  uint32_t auto_frequency_;
  uint32_t auto_target_frequencies_[OCTAVES + 1];
  int16_t auto_calibration_data_[OCTAVES + 1];
  uint32_t auto_last_frequency_;
  bool auto_next_step_;
  bool auto_error_;
  bool auto_ready_;
  bool autotune_completed_;
  uint32_t auto_freq_sum_;
  uint32_t auto_freq_count_;
  uint32_t ticks_since_last_freq_;
  uint32_t auto_num_passes_;
  uint16_t F_correction_factor_;
  bool correction_direction_;
  int16_t correction_cnt_positive_;
  int16_t correction_cnt_negative_;
  int16_t octaves_cnt_;
  // -----

  void Begin();
  void move_cursor(int offset);
  void change_value(int offset);
  void handleButtonLeft(const UI::Event &event);
  void handleButtonUp(const UI::Event &event);
  void handleButtonDown(const UI::Event &event);


  void autotuner_reset_completed() {
    autotune_completed_ = false;
  }

  inline uint8_t get_octave_cnt() {
    return octaves_cnt_ + 0x1;
  }

  inline uint8_t auto_tune_step() {
    return autotuner_step_;
  }

  void autotuner_arm(uint8_t _status) {
    reset_autotuner();
    armed_ = _status ? true : false;
  }
  
  void autotuner_run() {     
    SERIAL_PRINTLN("Starting autotuner...");
    autotuner_step_ = armed_ ? OC::DAC_VOLT_0_BASELINE : OC::DAC_VOLT_0_ARM;
    if (autotuner_step_ == OC::DAC_VOLT_0_BASELINE)
    // we start, so reset data to defaults:
      OC::DAC::set_default_channel_calibration_data(channel_);
  }

  void auto_reset_step() {
    SERIAL_PRINTLN("Autotuner reset step...");
    auto_num_passes_ = 0x0;
    auto_DAC_offset_error_ = 0x0;
    correction_direction_ = false;
    correction_cnt_positive_ = correction_cnt_negative_ = 0x0;
    F_correction_factor_ = 0xFF;
    auto_ready_ = false;
  }

  void reset_autotuner() {
    ticks_since_last_freq_ = 0x0;
    auto_frequency_ = 0x0;
    auto_last_frequency_ = 0x0;
    auto_error_ = 0x0;
    auto_ready_ = 0x0;
    armed_ = 0x0;
    autotuner_step_ = 0x0;
    F_correction_factor_ = 0xFF;
    correction_direction_ = false;
    correction_cnt_positive_ = 0x0;
    correction_cnt_negative_ = 0x0;
    octaves_cnt_ = 0x0;
    auto_num_passes_ = 0x0;
    auto_DAC_offset_error_ = 0x0;
    autotune_completed_ = 0x0;
    reset_calibration_data();
  }

  const uint32_t get_auto_frequency() {
    return auto_frequency_;
  }

  uint8_t _ready() {
     return auto_ready_;
  }

  void reset_calibration_data() {
    
    for (int i = 0; i < OCTAVES + 1; i++) {
      auto_calibration_data_[i] = 0;
      auto_target_frequencies_[i] = 0.0f;
    }
  }

  uint8_t data_available() {
    return OC::DAC::calibration_data_used(channel_);
  }

  void use_default() {
    OC::DAC::set_default_channel_calibration_data(channel_);
  }

  void use_auto_calibration() {
    OC::DAC::set_auto_channel_calibration_data(channel_);
  }
  
  bool auto_frequency() {

    bool _f_result = false;

    if (ticks_since_last_freq_ > ERROR_TIMEOUT) {
      auto_error_ = true;
    }
    
    if (FreqMeasure.available()) {
      
      auto_freq_sum_ = auto_freq_sum_ + FreqMeasure.read();
      auto_freq_count_ = auto_freq_count_ + 1;

      // take more time as we're converging toward the target frequency
      uint32_t _wait = (F_correction_factor_ == 0x1) ? (FREQ_MEASURE_TIMEOUT << 2) :  (FREQ_MEASURE_TIMEOUT >> 2);
  
      if (ticks_since_last_freq_ > _wait) {

        // store frequency, reset, and poke ui to preempt screensaver:
        auto_frequency_ = uint32_t(FreqMeasure.countToFrequency(auto_freq_sum_ / auto_freq_count_) * 1000);
        history_.Push(auto_frequency_);
        auto_freq_sum_ = 0;
        auto_ready_ = true;
        auto_freq_count_ = 0;
        _f_result = true;
        ticks_since_last_freq_ = 0x0;
        OC::ui._Poke();
        history_.Update();
      }
    }
    return _f_result;
  }

  void measure_frequency_and_calc_error() {

    switch(autotuner_step_) {

      case OC::DAC_VOLT_0_ARM:
      // do nothing
      break;
      case OC::DAC_VOLT_0_BASELINE:
      // 0V baseline / calibration point: in this case, we don't correct.
      {
        bool _update = auto_frequency();
        if (_update && auto_num_passes_ > kHistoryDepth) { 
          
          auto_last_frequency_ = auto_frequency_;
          uint32_t history[kHistoryDepth]; 
          uint32_t average = 0;
          // average
          history_.Read(history);
          for (uint8_t i = 0; i < kHistoryDepth; i++)
            average += history[i];
          // ... and derive target frequency at 0V
          auto_frequency_ = ((auto_frequency_ + average) / (kHistoryDepth + 1)); // 0V 
          SERIAL_PRINTLN("Baseline auto_frequency_ = %4.d", auto_frequency_);
          // reset step, and proceed:
          auto_reset_step();
          autotuner_step_++;
        }
        else if (_update) 
          auto_num_passes_++;
      }
      break;
      case OC::DAC_VOLT_TARGET_FREQUENCIES: 
      {
        #ifdef BUCHLA_SUPPORT
        
          switch(OC::DAC::get_voltage_scaling(channel_)) {
            
            case VOLTAGE_SCALING_1_2V_PER_OCT: // 1.2V/octave
            auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers_1V2[octaves_cnt_];
            break;
            case VOLTAGE_SCALING_2V_PER_OCT: // 2V/octave
            auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers_2V0[octaves_cnt_];
            break;
            default: // 1V/octave
            auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers[octaves_cnt_]; 
            break;
          }
        #else
          auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers[octaves_cnt_ + (5 - OC::DAC::kOctaveZero)]; 
        #endif
        octaves_cnt_++;
        // go to next step, if done:
        if (octaves_cnt_ > ACTIVE_OCTAVES) {
          octaves_cnt_ = 0x0;
          autotuner_step_++;
        }
      }
      break;
      case OC::DAC_VOLT_3m:
      case OC::DAC_VOLT_2m:
      case OC::DAC_VOLT_1m: 
      case OC::DAC_VOLT_0:
      case OC::DAC_VOLT_1:
      case OC::DAC_VOLT_2:
      case OC::DAC_VOLT_3:
      case OC::DAC_VOLT_4:
      case OC::DAC_VOLT_5:
      case OC::DAC_VOLT_6:
      #ifdef VOR
      case OC::DAC_VOLT_7:
      #endif
      { 
        bool _update = auto_frequency();
        
        if (_update && (auto_num_passes_ > MAX_NUM_PASSES)) {  
          /* target frequency reached */
          SERIAL_PRINTLN("* Target Frequency Reached *");
          
          if ((autotuner_step_ > OC::DAC_VOLT_2m) && (auto_last_frequency_ * 1.25f > auto_frequency_))
              auto_error_ = true; // throw error, if things don't seem to double ...

          // average:
          uint32_t history[kHistoryDepth]; 
          uint32_t average = 0;
          history_.Read(history);
          for (uint8_t i = 0; i < kHistoryDepth; i++)
            average += history[i];

          // store last frequency:
          auto_last_frequency_  = (auto_frequency_ + average) / (kHistoryDepth + 1);
          // and DAC correction value:
          auto_calibration_data_[autotuner_step_ - OC::DAC_VOLT_3m] = auto_DAC_offset_error_;

          // reset and step forward
          auto_reset_step();
          autotuner_step_++; 
        }
        else if (_update)
        {
          auto_num_passes_++; // count passes

          SERIAL_PRINTLN("auto_target_frequencies[%3d]_ = %3d", autotuner_step_ - OC::DAC_VOLT_3m,
                        auto_target_frequencies_[autotuner_step_ - OC::DAC_VOLT_3m] );
          SERIAL_PRINTLN("auto_frequency_ = %3d", auto_frequency_);

          // and correct frequency
          if (auto_target_frequencies_[autotuner_step_ - OC::DAC_VOLT_3m] > auto_frequency_)
          {
            // update correction factor?
            if (!correction_direction_)
              F_correction_factor_ = (F_correction_factor_ >> 1) | 1u;

            correction_direction_ = true;

            auto_DAC_offset_error_ += F_correction_factor_;

            // we're converging -- count passes, so we can stop after x attempts:
            if (F_correction_factor_ == 0x1) correction_cnt_positive_++;
          }
          else if (auto_target_frequencies_[autotuner_step_ - OC::DAC_VOLT_3m] < auto_frequency_)
          {
            // update correction factor?
            if (correction_direction_)
              F_correction_factor_ = (F_correction_factor_ >> 1) | 1u;

            correction_direction_ = false;
            
            auto_DAC_offset_error_ -= F_correction_factor_;

            // we're converging -- count passes, so we can stop after x attempts:
            if (F_correction_factor_ == 0x1) correction_cnt_negative_++;
          }

          SERIAL_PRINTLN("auto_DAC_offset_error_ = %3d", auto_DAC_offset_error_);

          // approaching target? if so, go to next step.
          if (correction_cnt_positive_ > CONVERGE_PASSES && correction_cnt_negative_ > CONVERGE_PASSES)
            auto_num_passes_ = MAX_NUM_PASSES << 1;
        }
      }
      break;
      case OC::AUTO_CALIBRATION_STEP_LAST:
      // step through the octaves:
      if (ticks_since_last_freq_ > 2000) {
        int32_t new_auto_calibration_point = OC::calibration_data.dac.calibrated_octaves[channel_][octaves_cnt_] + auto_calibration_data_[octaves_cnt_];
        // write to DAC and update data
        if (new_auto_calibration_point >= 65536 || new_auto_calibration_point < 0) {
          auto_error_ = true;
          autotuner_step_++;
        }
        else {
          OC::DAC::set(channel_, new_auto_calibration_point);
          OC::DAC::update_auto_channel_calibration_data(channel_, octaves_cnt_, new_auto_calibration_point);
          ticks_since_last_freq_ = 0x0;
          octaves_cnt_++;
        }
      }
      // then stop ... 
      if (octaves_cnt_ > OCTAVES) { 
        autotune_completed_ = true;
        // and point to auto data ...
        OC::DAC::set_auto_channel_calibration_data(channel_);
        autotuner_step_++;
      }
      break;
      default:
      autotuner_step_ = OC::DAC_VOLT_0_ARM;
      armed_ = 0x0;
      break;
    }
  }
  
  void autotune_updateDAC() {

    switch(autotuner_step_) {

      case OC::DAC_VOLT_0_ARM: 
      {
        F_correction_factor_ = 0x1; // don't go so fast
        auto_frequency();
        OC::DAC::set(channel_, OC::calibration_data.dac.calibrated_octaves[channel_][OC::DAC::kOctaveZero]);
      }
      break;
      case OC::DAC_VOLT_0_BASELINE:
      // set DAC to 0.000V, default calibration:
      OC::DAC::set(channel_, OC::calibration_data.dac.calibrated_octaves[channel_][OC::DAC::kOctaveZero]);
      break;
      case OC::DAC_VOLT_TARGET_FREQUENCIES:
      case OC::AUTO_CALIBRATION_STEP_LAST:
      // do nothing
      break;
      default: 
      // set DAC to calibration point + error
      {
        int32_t _default_calibration_point = OC::calibration_data.dac.calibrated_octaves[channel_][autotuner_step_ - OC::DAC_VOLT_3m];
        OC::DAC::set(channel_, _default_calibration_point + auto_DAC_offset_error_);
      }
      break;
    }
  }
};

  template <typename Owner>
  void Autotuner<Owner>::ISR() {
      if (armed_) {
          autotune_updateDAC();
          measure_frequency_and_calc_error();
          ticks_since_last_freq_++;
      }
  }

  template <typename Owner>
  void Autotuner<Owner>::Draw() {
    
    weegfx::coord_t w = 128;
    weegfx::coord_t x = 0;
    weegfx::coord_t y = 0;
    weegfx::coord_t h = 64;

    graphics.clearRect(x, y, w, h);
    graphics.drawFrame(x, y, w, h);
    graphics.setPrintPos(x + 2, y + 3);
    graphics.print(OC::Strings::channel_id[channel_]);

    x = 16; y = 15;

    if (auto_error_) {
      auto_tune_running_status_ = AT_ERROR;
      reset_autotuner();
    }
    else if (autotune_completed_) {
      auto_tune_running_status_ = AT_DONE;
      autotuner_reset_completed();
    }
    
    for (size_t i = 0; i < (AUTO_MENU_ITEMS_LAST - 0x1); ++i, y += 20) {
        //
      graphics.setPrintPos(x + 2, y + 4);
      
      if (i == DATA_SELECT) {
        graphics.print("use --> ");
        
        switch(calibration_data_) {
          case 0x0:
          graphics.print("(dflt.)");
          break;
          case 0x01:
          graphics.print("auto.");
          break;
          default:
          graphics.print("dflt.");
          break;
        }
      }
      else if (i == AUTOTUNE) {
        
        switch (auto_tune_running_status_) {
        //to display progress, if running
        case AT_OFF:
        graphics.print("run --> ");
        graphics.print(" ... ");
        break;
        case AT_READY: {
        graphics.print("arm > ");
        const uint32_t _freq = get_auto_frequency();
        if (_freq == 0)
          graphics.printf("wait ...");
        else 
        {
          const uint32_t value = _freq / 1000;
          const uint32_t cents = _freq % 1000;
          graphics.printf("%5u.%03u", value, cents);
        }
        }
        break;
        case AT_RUN:
        {
          int _octave = get_octave_cnt();
          if (_octave > 1 && _octave < OCTAVES) {
            for (int i = 0; i <= _octave; i++, x += 6)
              graphics.drawBitmap8(x + 18, y + 4, 4, OC::bitmap_indicator_4x8);
          }
          else if (auto_tune_step() == DAC_VOLT_0_BASELINE || auto_tune_step() == DAC_VOLT_TARGET_FREQUENCIES) // this goes too quick, so ... 
            graphics.print(" 0.0V baseline");
          else {
            graphics.print(AT_steps[auto_tune_step() - DAC_VOLT_3m]);
            if (!_ready())
              graphics.print(" ");
            else 
            {
              const uint32_t f = get_auto_frequency();
              const uint32_t value = f / 1000;
              const uint32_t cents = f % 1000;
              graphics.printf(" > %5u.%03u", value, cents);
            }
          }
        }
        break;
        case AT_ERROR:
        graphics.print("run --> ");
        graphics.print("error!");
        break;
        case AT_DONE: 
        graphics.print(OC::Strings::channel_id[channel_]);
        graphics.print("  --> a-ok!");
        calibration_data_ = data_available();
        break;
        default:
        break;
        }
      }
    }
 
    x = 16; y = 15;
    for (size_t i = 0; i < (AUTO_MENU_ITEMS_LAST - 0x1); ++i, y += 20) {
      
      graphics.drawFrame(x, y, 95, 16);
      // cursor:
      if (i == cursor_pos_) 
        graphics.invertRect(x - 2, y - 2, 99, 20);
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::HandleButtonEvent(const UI::Event &event) {
    
     if (UI::EVENT_BUTTON_PRESS == event.type) {
      switch (event.control) {
        case OC::CONTROL_BUTTON_UP:
          handleButtonUp(event);
          break;
        case OC::CONTROL_BUTTON_DOWN:
          handleButtonDown(event);
          break;
        case OC::CONTROL_BUTTON_L:
          handleButtonLeft(event);
          break;    
        case OC::CONTROL_BUTTON_R:
          reset_autotuner();
          Close();
          break;
        default:
          break;
      }
    }
    else if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
       switch (event.control) {
        case OC::CONTROL_BUTTON_UP:
          // screensaver 
        break;
        case OC::CONTROL_BUTTON_DOWN:
          OC::DAC::reset_all_auto_channel_calibration_data();
          calibration_data_ = 0x0;
        break;
        case OC::CONTROL_BUTTON_L: 
        break;
        case OC::CONTROL_BUTTON_R:
         // app menu
        break;  
        default:
        break;
      }
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::HandleEncoderEvent(const UI::Event &event) {
   
    if (OC::CONTROL_ENCODER_R == event.control) {
      move_cursor(event.value);
    }
    else if (OC::CONTROL_ENCODER_L == event.control) {
      change_value(event.value); 
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::move_cursor(int offset) {

    if (auto_tune_running_status_ < AT_RUN) {
      int cursor_pos = cursor_pos_ + offset;
      CONSTRAIN(cursor_pos, 0, AUTO_MENU_ITEMS_LAST - 0x2);  
      cursor_pos_ = cursor_pos;
      //
      if (cursor_pos_ == DATA_SELECT)
          auto_tune_running_status_ = AT_OFF;
    }
    else if (auto_tune_running_status_ == AT_ERROR || auto_tune_running_status_ == AT_DONE)
      auto_tune_running_status_ = AT_OFF;
  }

  template <typename Owner>
  void Autotuner<Owner>::change_value(int offset) {

    switch (cursor_pos_) {
      case DATA_SELECT:
      {
        uint8_t data = data_available();
        if (!data) { // no data -- 
          calibration_data_ = 0x0;
          data_select_ = 0x0;
        }
        else {
          int _data_sel = data_select_ + offset;
          CONSTRAIN(_data_sel, 0, 0x1);  
          data_select_ = _data_sel;
          if (_data_sel == 0x0) {
            calibration_data_ = 0xFF;
            use_default();
          }
          else {
            calibration_data_ = 0x01;
            use_auto_calibration();
          }
        }
      }
      break;
      case AUTOTUNE: 
      {
        if (auto_tune_running_status_ < AT_RUN) {
          int _status = auto_tune_running_status_ + offset;
          CONSTRAIN(_status, 0, AT_READY);
          auto_tune_running_status_ = AT_STATUS(_status);
          autotuner_arm(_status);
        }
        else if (auto_tune_running_status_ == AT_ERROR || auto_tune_running_status_ == AT_DONE)
          auto_tune_running_status_ = AT_OFF;
      }
      break;
      default:
      break;
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonUp(const UI::Event &event) {

    if (cursor_pos_ == AUTOTUNE && auto_tune_running_status_ == AT_OFF) {
      // arm the tuner
      auto_tune_running_status_ = AT_READY;
      autotuner_arm(auto_tune_running_status_);
    }
    else {
      reset_autotuner();
      auto_tune_running_status_ = AT_OFF;
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonDown(const UI::Event &event) {
    
    if (cursor_pos_ == AUTOTUNE && auto_tune_running_status_ == AT_READY) {
      autotuner_run();
      auto_tune_running_status_ = AT_RUN;
    }
    else if (auto_tune_running_status_ == AT_ERROR) {
      reset_autotuner();
      auto_tune_running_status_ = AT_OFF;
    }  
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonLeft(const UI::Event &) {
  }
  
  template <typename Owner>
  void Autotuner<Owner>::Begin() {
    
    const OC::Autotune_data &autotune_data = OC::AUTOTUNE::GetAutotune_data(channel_);
    calibration_data_ = autotune_data.use_auto_calibration_;
    
    if (calibration_data_ == 0x01) // auto cal. data is in use
      data_select_ = 0x1;
    else
      data_select_ = 0x00;
    cursor_pos_ = 0x0;
    auto_tune_running_status_ = AT_OFF;
  }
  
  template <typename Owner>
  void Autotuner<Owner>::Close() {
    ui.SetButtonIgnoreMask();
    owner_->ExitAutotune();
    owner_ = nullptr;
  }
}; // namespace OC

#endif // OC_AUTOTUNER_H
