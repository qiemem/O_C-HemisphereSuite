#include "HemisphereApplet.h"

HS::IOFrame HS::frame;
HS::ClockManager HS::clock_m;

int HemisphereApplet::cursor_countdown[APPLET_SLOTS];

void HemisphereApplet::BaseStart(HEM_SIDE hemisphere_) {
    hemisphere = hemisphere_;

    // Initialize some things for startup
    full_screen = 0;
    cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;

    // Shutdown FTM capture on Digital 4, used by Tuner
#ifdef FLIP_180
    if (hemisphere == 0)
#else
    if (hemisphere == 1)
#endif
    {
        FreqMeasure.end();
        OC::DigitalInputs::reInit();
    }

    // Maintain previous app state by skipping Start
    if (!applet_started) {
        applet_started = true;
        Start();
        ForEachChannel(ch) {
            Out(ch, 0); // reset outputs
        }
    }
}

void HemisphereApplet::BaseController() {
    // I moved the IO-related stuff to the parent HemisphereManager app.
    // The IOFrame gets loaded before calling Controllers, and outputs are handled after.
    // -NJM

    // Cursor countdowns. See CursorBlink(), ResetCursor(), gfxCursor()
    if (--cursor_countdown[hemisphere] < -HEMISPHERE_CURSOR_TICKS) cursor_countdown[hemisphere] = HEMISPHERE_CURSOR_TICKS;

    Controller();
}

void HemisphereApplet::BaseView() {
    //if (HS::select_mode == hemisphere)
    gfxHeader(applet_name());
    // If active, draw the full screen view instead of the application screen
    if (full_screen) this->DrawFullScreen();
    else this->View();
}

/*
 * Has the specified Digital input been clocked this cycle?
 *
 * If physical is true, then logical clock types (master clock forwarding and metronome) will
 * not be used.
 *
 * You DON'T usually want to call this more than once per tick for each channel!
 * It modifies state by eating boops and updating cycle_ticks. -NJM
 */
bool HemisphereApplet::Clock(int ch, bool physical) {
    bool clocked = 0;
    bool useTock = (!physical && HS::clock_m.IsRunning());

#ifdef ARDUINO_TEENSY41
    const size_t virt_chan = (ch + io_offset) % 8;
#else
    const size_t virt_chan = (ch + io_offset) % 4;
#endif

    // clock triggers
    if (useTock && HS::clock_m.GetMultiply(virt_chan) != 0)
        clocked = HS::clock_m.Tock(virt_chan);
    else if (trigger_mapping[ch + io_offset] > 0 && trigger_mapping[ch + io_offset] <= ADC_CHANNEL_LAST)
        clocked = frame.clocked[ trigger_mapping[ch + io_offset] - 1 ];

    // Try to eat a boop
    clocked = clocked || clock_m.Beep(virt_chan);

    if (clocked) {
        frame.cycle_ticks[io_offset + ch] = OC::CORE::ticks - frame.last_clock[io_offset + ch];
        frame.last_clock[io_offset + ch] = OC::CORE::ticks;
    }
    return clocked;
}
