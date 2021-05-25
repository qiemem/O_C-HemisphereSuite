#include <cstdint>
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

const int NGRAM_BUFF_LEN = 64;
const int NGRAM_PATT_LEN = 8;

// based on https://github.com/rlogiacco/CircularBuffer
template<typename T, size_t N>
class CircularBuffer {
public:
    constexpr CircularBuffer() : head(buffer), tail(buffer), count(0) { }

    bool push(T value) {
        if (++tail == buffer + N) {
            tail = buffer;
        }
        *tail = value;
        if (count == N) {
            if (++head == buffer + N) {
                head = buffer;
            }
            return false;
        } else {
            if (count++ == 0) {
                head = tail;
            }
            return true;
        }
    }

    T operator [](size_t index) const {
        if (index < 0) {
            index = count + index;
        }
        return index >= count ? *tail : *(buffer + ((head - buffer + index) % N));
    }

    T inline first() const {
        return *head;
    }

    T inline last() const {
        return *tail;
    }

    bool isEmpty() {
        return count == 0;
    }

    size_t size() {
        return count;
    }

    void clear() {
        tail = head;
        count = 0;
    }
private:
    T buffer[N];
    T* head;
    T* tail;
    size_t count;
};

class Ngram : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Ngram";
    }

    void Start() {
        quantizer.Init();
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);
    }

    void Controller() {
        if (Clock(1)) {
            if (learn) {
                buff.push(quantizer.Process(In(1), root << 7, 0));
                sampled_ix--;
            }
        }
        if (Clock(0) && !buff.isEmpty()) {
            sampled_ix = Sample();
            int sample = buff[sampled_ix];
            //int sample = buff.first();
            //int sample = buff[random(0, buff.size())];
            //int sample = buff.last();
            patt.push(sample);
            Out(0, sample);
            Out(1, buff.last());
        }
    }

    void View() {
        gfxHeader(applet_name());

        gfxPrint(0, 15, "N: ");
        gfxPrint(patt_size);
        gfxPrint(" ");
        if (!buff.isEmpty()) {
            gfxPrint(0, 25, buff.first());
            gfxPrint(0, 35, buff.last());
        }

        if (cursor == 0) {
            gfxCursor(26, 23, 30);
        }

        for (size_t i=0; i < buff.size(); i++) {
            gfxPixel(i, 53 - int(8 * float(buff[i]) / HEMISPHERE_MAX_CV));
            if (i == sampled_ix) {
                gfxLine(i, 53, i, 45);
            }
        }
        for (size_t i=0; i < patt.size(); i++) {
            gfxPixel(i, 63 - int(8 * float(patt[i]) / HEMISPHERE_MAX_CV));
        }

    }

    void OnButtonPress() {
    }

    void OnEncoderMove(int direction) {
        patt_size = hem_MIN(hem_MAX(patt_size + direction, 1), 8);
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        //Pack(data, PackLocation {0,7}, p[0]);
        //Pack(data, PackLocation {7,7}, p[1]);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        //p[0] = Unpack(data, PackLocation {0,7});
        //p[1] = Unpack(data, PackLocation {7,7});
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "Clock Ch1, Ch2";
        help[HEMISPHERE_HELP_CVS]      = "p Mod Ch1, Ch2";
        help[HEMISPHERE_HELP_OUTS]     = "Clock Ch1, Ch2";
        help[HEMISPHERE_HELP_ENCODER]  = "Set p";
    }

private:
    braids::Quantizer quantizer;
    int scale = 5;
    int root = 0;

    CircularBuffer<int, NGRAM_BUFF_LEN> buff;
    CircularBuffer<int, NGRAM_PATT_LEN> patt;
    uint8_t patt_size = 1;
    uint8_t smoothing = 0;
    size_t sampled_ix = -1;
    bool learn = true;

    uint8_t cursor;

    int Sample() {
        if (random(0, 99) < smoothing) {
            //return buff[random(0, buff.size())];
            return random(buff.size());
        }

        //size_t sample = random(buff.size());//buff.size() - 1;//buff.last();
        size_t sample = buff.size() - 1;
        size_t n = hem_MIN(patt_size, patt.size());
        size_t matches = 0;
        size_t m = buff.size();
        size_t best_match = 0;
        for (size_t i = n; i < m; i++) {
            bool match = true;
            for (size_t j = 1; j <= n; j++) {
                if (buff[i - j] != patt[patt.size()-j]) {
                    match = false;
                    break;
                }
                if (best_match < j) {
                    best_match = j;
                    matches = 0;
                }
            }
            if (match) {
                if (random(++matches) == 0) {
                    //sample = buff[i + n];
                    sample = i;
                }
            }
        }
        return sample;
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to Ngram,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
Ngram Ngram_instance[2];

void Ngram_Start(bool hemisphere) {
    Ngram_instance[hemisphere].BaseStart(hemisphere);
}

void Ngram_Controller(bool hemisphere, bool forwarding) {
    Ngram_instance[hemisphere].BaseController(forwarding);
}

void Ngram_View(bool hemisphere) {
    Ngram_instance[hemisphere].BaseView();
}

void Ngram_OnButtonPress(bool hemisphere) {
    Ngram_instance[hemisphere].OnButtonPress();
}

void Ngram_OnEncoderMove(bool hemisphere, int direction) {
    Ngram_instance[hemisphere].OnEncoderMove(direction);
}

void Ngram_ToggleHelpScreen(bool hemisphere) {
    Ngram_instance[hemisphere].HelpScreen();
}

uint32_t Ngram_OnDataRequest(bool hemisphere) {
    return Ngram_instance[hemisphere].OnDataRequest();
}

void Ngram_OnDataReceive(bool hemisphere, uint32_t data) {
    Ngram_instance[hemisphere].OnDataReceive(data);
}
