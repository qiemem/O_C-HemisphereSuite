#pragma once

#include "CVInputMap.h"
#include "HemisphereApplet.h"
#include "dsputils.h"
#include <AudioStream.h>
#include <cstdint>
#include <variant>

// For ascii strings of 9 characters or less, will just be the ascii bits
// concatenated together. More characters than that and the xor plus misaligned
// shifting should avoid collisions.
constexpr uint64_t strhash(const char* str) {
  uint64_t id = 0;
  for (const char* c = str; *c != '\0'; c++) {
    id = (id << 7) | (id >> (64 - 7));
    id ^= (*c);
  }
  return id;
}

enum AudioChannels : uint8_t {
  NONE,
  MONO,
  STEREO,
};

class HemisphereAudioApplet : public HemisphereApplet {
public:
  static const uint_fast8_t CONFIG_SIZE = 4;

  // -90 = 15bits of depth so no point in going lower
  static const int LVL_MIN_DB = -90;
  static const int LVL_MAX_DB = 90;

  // If applet_name() can return different things at different times, you
  // *must* override this or saving and loading won't work!
  virtual const uint64_t applet_id() {
    return strhash(applet_name());
  };
  virtual AudioStream* InputStream() = 0;
  virtual AudioStream* OutputStream() = 0;
  virtual void mainloop() {}

  virtual void OnDataReceive(uint64_t data) {
    Serial.println(
      "Warning: default OnDataReceive() called; either override this or OnDataReceive(const std::<array<uint64_t, CONFIG_SIZE>& data)"
    );
  }
  virtual uint64_t OnDataRequest() {
    Serial.println(
      "Warning: default OnDataRequest() called; either override this or OnDataRequest(std::<array<uint64_t, CONFIG_SIZE>& data)"
    );
    return 0;
  }
  virtual void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) {
    OnDataReceive(data[0]);
  }
  virtual void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) {
    data[0] = OnDataRequest();
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
  }

  void gfxPrintTuningIndicator(int16_t pitch) {
    // TODO this assumes pitch = C, which might not be true for some applets
    int semitone = pitch / 128;
    int offset = pitch - semitone * 128;
    if (offset >= 64) {
      offset = offset - 128;
      semitone++;
    }
    semitone = ((semitone % 12) + 12) % 12;
    int y = gfxGetPrintPosY();
    int x = gfxGetPrintPosX();
    gfxPos(x + 1, y);
    gfxPrintIcon(NOTE_NAMES + semitone * 8, 9);
    int pxOffset = 7 - (offset / 16 + 4);
    if (offset == 0) gfxInvert(x, y, 10, 8);
    else gfxDottedLine(x, y + pxOffset, x + 10, y + pxOffset);
  }

  void gfxPrintPitchHz(int16_t pitch, float base_freq = C3) {
    float freq = PitchToRatio(pitch) * base_freq;
    int shiftedFreq = static_cast<int>(roundf(freq * 10));
    int int_part = shiftedFreq / 10;
    int dec = shiftedFreq % 10;
    if (int_part > 9999) graphics.printf("%6d", int_part);
    else graphics.printf("%4d.%01d", int_part, dec);
    gfxPrintIcon(HZ);
  }

  void gfxPrintDb(int db) {
    if (db < LVL_MIN_DB) gfxPrint("    - ");
    else graphics.printf("%3ddB", db);
  }

  bool EditInputMap(CVInputMap& input_map) {
    if (!IsEditingInputMap()) {
      selected_input_map = &input_map;
      return true;
    }
    return false;
  }

  bool EditInputMap(DigitalInputMap& input_map) {
    if (!IsEditingInputMap()) {
      selected_input_map = &input_map;
      return true;
    }
    return false;
  }

  void ClearEditInputMap() {
    selected_input_map = std::monostate{};
    if (EditMode()) CursorToggle();
  }

  bool EditSelectedInputMap(int direction) {
    if (IsEditingInputMap()) {
      switch (selected_input_map.index()) {
        case CV_INPUT_MAP: {
          int8_t& att
            = std::get<CVInputMap*>(selected_input_map)->attenuversion;
          att = constrain(att + direction, -100, 100);
          break;
        }
        case DIGITAL_INPUT_MAP: {
          int8_t& div
            = std::get<DigitalInputMap*>(selected_input_map)->division;
          div = constrain(div + direction, -64, 64);
          break;
        }
        default:
          break;
      }
      return true;
    }
    return false;
  }

  void gfxDisplayInputMapEditor() {
    if (selected_input_map.index()) {
      gfxClear(0, 0, 63, 11);
      switch (selected_input_map.index()) {
        case CV_INPUT_MAP: {
          gfxPos(32 - 5 * 6 / 2, 2);
          graphics.printf(
            "%4d%%", std::get<CVInputMap*>(selected_input_map)->attenuversion
          );
          break;
        }
        case DIGITAL_INPUT_MAP: {
          gfxPos(32 - 4 * 6 / 2, 2);
          int8_t div = std::get<DigitalInputMap*>(selected_input_map)->division;
          if (div < 0) graphics.printf("/%3d", -div + 1);
          else graphics.printf("X%3d", div + 1);
          break;
        }
        default:
          break;
      }
      gfxInvert(0, 0, 63, 11);
    }
  }

  bool IsEditingInputMap() const {
    return selected_input_map.index() > 0;
  }

  template <typename T>
  constexpr std::pair<int, T&&> IndexedInput(int index, T&& input_map) {
    return std::pair<int, T&&>(index, std::forward<T>(input_map));
  }

  template <typename... Pairs>
  bool CheckEditInputMapPress(int cursor, Pairs&&... indexed_input_maps) {
    if (IsEditingInputMap()) {
      ClearEditInputMap();
      return !EditMode();
    } else if (!EditMode()) {
      return false;
    }

    return (
      ...
      || (cursor == indexed_input_maps.first ? EditInputMap(indexed_input_maps.second) : false)
    );
  }

protected:
  enum SelectedInputMapType {
    NONE,
    CV_INPUT_MAP,
    DIGITAL_INPUT_MAP,
  };

  std::variant<std::monostate, CVInputMap*, DigitalInputMap*>
    selected_input_map;
};
