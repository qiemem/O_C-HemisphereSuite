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

    bool isEmpty() const {
        return count == 0;
    }

    size_t size() const {
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

class Ngram : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Ngram";
    }

    void Start() {
        quantizer.Init();
        set_scale(scale);
    }

    void Controller() {
        ForEachChannel (ch) if (Clock(ch)) StartADCLag(ch);

        int input = learn_mode == FEEDBACK ? patt.last() : In(1);
        bool learn = false;
        bool reset = false;
        // Use end here because the newest learned value should be used
        bool play_next = EndOfADCLag(0);
        smoothing_cv = Proportion(DetentedIn(0), HEMISPHERE_MAX_CV, NGRAM_MAX_SMOOTHING);
        switch (learn_mode) {
            case FEEDBACK:
            case AUTO:
                reset = Clock(1);
                learn = play_next;
                break;
            case MANUAL:
                learn = EndOfADCLag(1);
                break;
            case OFF:
                break;
        }

        if (reset) {
            buff.clear();
            patt.clear();
            ResetSampler();
        }

        if (learn) {
            if (scale > 0) {
                buff.push(quantizer.Process(input, root << 7, 0));
            } else {
                buff.push(input > HEMISPHERE_3V_CV ? HEMISPHERE_MAX_CV : 0);
            }
            if (last_sampled_ix > 0) last_sampled_ix--;

            // The sampled value might drop off the beginning of the buffer;
            // that's okay; we basically just virtually extend the buffer
            // length until we play. We could just try to re-adjust everything
            // to remove the effect of the lost value on score totals, and
            // approximate resampling or something, but it would be a total
            // pain. This is pretty reasonable behavior and is way simpler.
            if (sampled_ix > 0) sampled_ix--;
            if (sampler_ix > 0) sampler_ix--;

            if (SampleIndex(patt, buff.size() - 1, total)) {
                sampled_ix = buff.size() - 1;
                sampled_value = buff[sampled_ix];
            }
        }

        int iters = 8;
        while (iters-- && sampler_ix > 0) {
            if (SampleIndex(patt, --sampler_ix, total)) {
                sampled_ix = sampler_ix;
                sampled_value = buff[sampled_ix];
            }
        }


        if (play_next && !buff.isEmpty()) {

            if (last_sampled_ix < buff.size() - 1
                    && buff[last_sampled_ix + 1] == sampled_value) {
                sampled_ix = last_sampled_ix + 1;
            }
            last_sampled_ix = sampled_ix;
            patt.push(sampled_value);


            if (scale > 0) {
                Out(0, sampled_value);
            } else if (sampled_value >= HEMISPHERE_3V_CV) {
                ClockOut(0);
            }
            if (scale > 0) {
                Out(1, buff.last());
            } else if (buff.last() >= HEMISPHERE_3V_CV) {
                ClockOut(1);
            }

            ResetSampler();
        }
    }

    void View() {
        gfxHeader(applet_name());

        if (scale > 0) {
            gfxPrint(0, 15, OC::scale_names_short[scale - 1]);
        } else {
            gfxPrint(0, 15, "TRIG");
        }
        if (cursor == SCALE) gfxCursor(0, 23, 30);
        gfxPrint(36, 15, OC::Strings::note_names_unpadded[root]);
        if (cursor == ROOT) gfxCursor(36, 23, 12);

        gfxPrint(0, 25, "Wght: ");
        for (size_t i = 0; i < NGRAM_PATT_LEN; i++) {
            int height = weight(NGRAM_PATT_LEN - i);
            gfxRect(32 + i * 4, 32 - height, 3, height);
        }
        if (cursor == SMOOTH) gfxCursor(32, 33, 32);

        gfxPrint(0, 35, "Lrn: ");
        switch (learn_mode) {
            case AUTO: gfxPrint("Auto"); break;
            case MANUAL: gfxPrint("Man"); break;
            case FEEDBACK: gfxPrint("Fdbck"); break;
            case OFF: gfxPrint("Off"); break;
        }
        if (cursor == LEARN) gfxCursor(30, 43, 33);


        int max_cv = -HEMISPHERE_3V_CV;
        int min_cv = HEMISPHERE_MAX_CV;

        for (size_t i=0; i < buff.size(); i++) {
            if (buff[i] > max_cv) {
                max_cv = buff[i];
            }
            if (buff[i] < min_cv) {
                min_cv = buff[i];
            }
        }
        for (size_t i=0; i < patt.size(); i++) {
            if (patt[i] > max_cv) {
                max_cv = patt[i];
            }
            if (patt[i] < min_cv) {
                min_cv = patt[i];
            }
        }
        const int cv_range = max_cv - min_cv + 1;

        const int TRACK_HEIGHT = 8;
        const int BUFF_Y = 45;
        const int PATT_Y = 55;

        for (size_t i=0; i < buff.size(); i++) {
            int v = Proportion(buff[i] - min_cv, cv_range, TRACK_HEIGHT);
            gfxLine(i, BUFF_Y + TRACK_HEIGHT - 1 - v, i, BUFF_Y + TRACK_HEIGHT - 1);
            if (i == last_sampled_ix) {
                gfxInvert(i, BUFF_Y, 1, TRACK_HEIGHT);
            }
        }

        for (size_t i=0; i < patt.size(); i++) {
            int v = Proportion(patt[i] - min_cv, cv_range, TRACK_HEIGHT);
            gfxLine(i, PATT_Y + TRACK_HEIGHT - 1 - v, i, PATT_Y + TRACK_HEIGHT - 1);
        }

        int patt_border_w = hem_MAX(patt.size() - NGRAM_PATT_LEN - 1, 0);
        int patt_width = hem_MAX(hem_MIN(NGRAM_PATT_LEN, patt.size() - 1), 0);

        gfxLine(patt_border_w, PATT_Y - 1,
                patt_border_w, PATT_Y + TRACK_HEIGHT);
        gfxLine(patt_border_w, PATT_Y - 1,
                patt_border_w + patt_width, PATT_Y - 1);
        gfxLine(patt_border_w, PATT_Y + TRACK_HEIGHT,
                patt_border_w + patt_width, PATT_Y + TRACK_HEIGHT);

        if (cursor == RESET && CursorBlink()) {
            gfxLine(0, BUFF_Y, 63, 63);
            gfxLine(63, BUFF_Y, 0, 63);
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
            case SMOOTH: set_smoothing(smoothing + direction); break;
            case LEARN: set_learn_mode(learn_mode + direction); break;
            case RESET: buff.clear(); patt.clear(); break;
        }
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0, 8}, scale);
        Pack(data, PackLocation {8, 4}, root);
        Pack(data, PackLocation {12, 6}, smoothing);
        Pack(data, PackLocation {18, 2}, learn_mode);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        set_scale(Unpack(data, PackLocation {0, 8}));
        set_root(Unpack(data, PackLocation {8, 4}));
        set_smoothing(Unpack(data, PackLocation {12, 6}));
        set_learn_mode(Unpack(data, PackLocation {18, 2}));
    }

protected:
    void SetHelp() {
        help[HEMISPHERE_HELP_DIGITALS] = "Sample, Lrn/Rst";
        help[HEMISPHERE_HELP_CVS]      = "Weight, Signal";
        help[HEMISPHERE_HELP_OUTS]     = "Sampled out, Thru";
        help[HEMISPHERE_HELP_ENCODER]  = "Scl,root,wght,lrn";
    }

    void set_scale(const int new_scale) {
        scale = mod(new_scale, OC::Scales::NUM_SCALES + 1);
        if (scale != 0) {
            quantizer.Configure(OC::Scales::GetScale(scale - 1), 0xffff);
        }
    }

    void set_root(const int new_root) {
        root = constrain(new_root, 0, 11);
    }

    void set_smoothing(const int new_smoothing) {
        smoothing = constrain(new_smoothing, 0, NGRAM_MAX_SMOOTHING);
    }

    void set_learn_mode(const int new_learn_mode) {
        learn_mode = (LearnMode) constrain(new_learn_mode, 0, OFF);
    }

private:

    const static size_t NGRAM_BUFF_LEN = 64;
    const static size_t NGRAM_PATT_LEN = 8;
    const static int NGRAM_MAX_SMOOTHING = 35;

    braids::Quantizer quantizer;
    int scale = 6; // SEMI
    int root = 0;

    CircularBuffer<int16_t, NGRAM_BUFF_LEN> buff;
    CircularBuffer<int16_t, NGRAM_BUFF_LEN> patt;
    uint8_t smoothing = 16;
    int8_t smoothing_cv = 0;

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
        SMOOTH,
        LEARN,
        RESET,
        CM_LAST
    };
    uint8_t cursor;

    size_t last_sampled_ix = 0;
    int sampled_value = 0;
    size_t sampled_ix = 0;
    size_t sampler_ix = 0;
    uint32_t total = 0;
    const static size_t MAX_SAMPLER_ITERS = NGRAM_BUFF_LEN;

    bool SampleIndex(const CircularBuffer<int16_t, NGRAM_BUFF_LEN>& pattern, size_t index, uint32_t& total) {
        uint32_t score = ScoreMatch(pattern, index);
        total += score;
        return random(total) < score;
    }

    uint32_t ScoreMatch(const CircularBuffer<int16_t, NGRAM_BUFF_LEN>& pattern, size_t index) {
        size_t n = hem_MAX(
                hem_MIN(
                    hem_MIN(NGRAM_PATT_LEN, pattern.size()),
                    buff.size() - 1),
                0);
        uint32_t score = 1;
        for (size_t j = 1; j <= n && j <= index; j++) {
            if (buff[index - j] == patt[patt.size() - j]) {
                // Because pow causes code size to increase too much...
                // This definitely isn't pow, but it gets at the same idea
                // At max weight, a match will increase chance of being by 8x
                score *= weight(j);
            }
        }
        return score;
    }

    void ResetSampler() {
        sampled_ix = hem_MAX(buff.size() - 1, 0);
        sampler_ix = hem_MAX(buff.size() - 1, 0);
        sampled_value = buff.isEmpty() ? 0 : buff[sampled_ix];
        total = buff.isEmpty() ? 0 : ScoreMatch(patt, sampler_ix);
    }


    // Weight with a flipped sigmoid offset by smoothing such that the higher
    // the smoothing and the higher the index, the closer to 0 and vice versa
    // for 1.
    // i should range from 1 to NGRAM_PATT_LEN, where this is how far back this
    // particular match is from the sample we're considering.
    uint32_t weight(int i) const {
        uint8_t s = constrain(smoothing + smoothing_cv, 0, NGRAM_MAX_SMOOTHING);
        return constrain(36 - s - 4 * (i - 1), 1, 8);
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
