#include <cstdint>
#include "braids_quantizer.h"
#include "braids_quantizer_scales.h"
#include "OC_scales.h"

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

int mod(int n, int q) {
    int m = n % q;
    if (m < 0) {
        m += q;
    }
    return m;
}

uint8_t ilog2(int n) {
    uint8_t i = 0;
    while (n >>= 1) i++;
    return i;
}

class Ngram : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Ngram";
    }

    void Start() {
        quantizer.Init();
        // init quantizer
        set_scale(scale);
    }

    void Controller() {
        int input = learn_mode == FEEDBACK ? patt.last() : In(1);
        bool learn = false;
        bool reset = false;
        switch (learn_mode) {
            case FEEDBACK:
            case AUTO:
                reset = Clock(1);
                learn = Clock(0);
                break;
            case MANUAL:
                learn = Clock(1);
                break;
            case OFF:
                break;
        }

        if (reset) {
            buff.clear();
            patt.clear();
        }

        if (learn) {
            buff.push(quantizer.Process(input, root << 7, 0));
            sampled_ix--;
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

        gfxPrint(0, 15, OC::scale_names_short[scale]);
        if (cursor == SCALE) gfxCursor(0, 23, 30);
        gfxPrint(36, 15, OC::Strings::note_names_unpadded[root]);
        if (cursor == ROOT) gfxCursor(36, 23, 12);

        gfxPrint(0, 25, "N: ");
        gfxPrint(patt_size);
        if (cursor == N) gfxCursor(18, 33, 6);
        gfxPrint(" P: ");
        gfxPrint(smoothing);
        if (cursor == SMOOTH) gfxCursor(48, 33, 6);

        gfxPrint(0, 35, "Lrn: ");
        switch (learn_mode) {
            case AUTO: gfxPrint("Auto"); break;
            case MANUAL: gfxPrint("Man"); break;
            case FEEDBACK: gfxPrint("Fdbck"); break;
            case OFF: gfxPrint("Off"); break;
        }
        if (cursor == LEARN) gfxCursor(30, 43, 33);

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
        cursor++;
        if (cursor == CM_LAST) {
            cursor = 0;
        }
    }

    void OnEncoderMove(int direction) {
        switch (cursor) {
            case SCALE: set_scale(scale + direction); break;
            case ROOT: set_root(root + direction); break;
            case N: set_patt_size(patt_size + direction); break;
            case SMOOTH: set_smoothing(smoothing + direction); break;
            case LEARN: set_learn_mode(learn_mode + direction); break;
        }
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0, 8}, scale);
        Pack(data, PackLocation {8, 4}, root);
        Pack(data, PackLocation {12, 4}, patt_size);
        Pack(data, PackLocation {16, 4}, smoothing);
        Pack(data, PackLocation {20, 2}, learn_mode);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        set_scale(Unpack(data, PackLocation {0, 8}));
        set_root(Unpack(data, PackLocation {8, 4}));
        set_patt_size(Unpack(data, PackLocation {12, 4}));
        set_smoothing(Unpack(data, PackLocation {16, 4}));
        set_learn_mode(Unpack(data, PackLocation {20, 2}));
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "Sample, Lrn/Rst";
        help[HEMISPHERE_HELP_CVS]      = "P, Signal";
        help[HEMISPHERE_HELP_OUTS]     = "Sampled out, Thru";
        help[HEMISPHERE_HELP_ENCODER]  = "Scl,root,n,p,lrn";
    }

    void set_scale(const int new_scale) {
        scale = mod(new_scale, OC::Scales::NUM_SCALES);
        quantizer.Configure(OC::Scales::GetScale(scale), 0xffff);
    }

    void set_root(const int new_root) {
        root = constrain(new_root, 0, 11);
    }

    void set_patt_size(const int new_patt_size) {
        patt_size = constrain(new_patt_size, 0, NGRAM_PATT_LEN);
    }

    void set_smoothing(const int new_smoothing) {
        smoothing = constrain(new_smoothing, 0, NGRAM_PATT_LEN);
    }

    void set_learn_mode(const int new_learn_mode) {
        learn_mode = (LearnMode) constrain(new_learn_mode, 0, OFF);
    }

private:

    const static int NGRAM_BUFF_LEN = 64;
    const static int NGRAM_PATT_LEN = 8;

    braids::Quantizer quantizer;
    int scale = 5;
    int root = 0;

    CircularBuffer<int16_t, NGRAM_BUFF_LEN> buff;
    CircularBuffer<int16_t, NGRAM_BUFF_LEN> patt;
    uint8_t patt_size = 1;
    uint8_t smoothing = 0;

    enum LearnMode {
        FEEDBACK,
        AUTO,
        MANUAL,
        OFF
    };
    LearnMode learn_mode = AUTO;

    enum CursorMode {
        SCALE,
        ROOT,
        N,
        SMOOTH,
        LEARN,
        CM_LAST
    };
    uint8_t cursor;

    size_t sampled_ix = -1;

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
