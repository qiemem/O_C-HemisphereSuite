My (qiemem) personal fork of Hemisphere primarily for the purpose of trying out new applet ideas, making tweaks to existing applets that I find useful, and pulling in changes from others that I like.
Originally forked from Logarhythm1's fork of Hemisphere, then updated with much of benirose's Benisphere, and now mostly based on djphazer's Phazerville...
djphazer's done an excellent job of tracking credit for all the various contributions to this ecosystem, so his original README below:

"What's the worst that could happen?"
===

## Phazerville Suite - an active o_C firmware fork

Using [Benisphere](https://github.com/benirose/O_C-BenisphereSuite) as a starting point, this branch takes the Hemisphere Suite in new directions, with several new applets and enhancements to existing ones, with the goal of cramming as much functionality and flexibility into the nifty dual-applet design as possible.

I've merged bleeding-edge features from various other branches, and confirmed that it compiles and runs on my uo_C.

### Notable Features in this branch:

* Improved internal clock controls, external clock sync, independent multipliers for each Hemisphere, MIDI Clock out via USB
* LoFi Tape has been transformed into LoFi Echo (credit to [armandvedel](https://github.com/armandvedel/O_C-HemisphereSuite_log) for the initial idea)
* ShiftReg has been upgraded to DualTM - two concurrent 32-bit registers governed by the same length/prob/scale/range settings, both outputs configurable to Pitch, Mod, Trig, Gate from either register
* EuclidX - AnnularFusion got a makeover, now includes configurable CV input modulation (credit to [qiemem](https://github.com/qiemem/O_C-HemisphereSuite/tree/expanded-clock-div) and [adegani](https://github.com/adegani/O_C-HemisphereSuite))
* Sequence5 -> SequenceX (8 steps max) (from [logarhythm](https://github.com/Logarhythm1/O_C-HemisphereSuite))
* EbbAndLfo (via [qiemem](https://github.com/qiemem/O_C-HemisphereSuite/tree/trig-and-tides)) - mini implementation of MI Tides, with v/oct tracking
* Modal-editing style navigation (push to toggle editing)
* other small tweaks + experimental applets

### How do I try it?

I might release a .hex file if there is demand... but I think the beauty of this module is the fact that it's relatively easy to modify and build the source code to reprogram it. You are free to customize the firmware, similar to how you've no doubt already selected a custom set of physical modules.

### How do I build it?

Building the code is fairly simple using Platform IO, a Python-based build toolchain, available as either a [standalone CLI](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or a [plugin within VSCode](https://platformio.org/install/ide?install=vscode). The project lives within the `software/o_c_REV` directory. From there, you can Build and Upload via USB to your module.

### Credits

This is a fork of [Benisphere Suite](https://github.com/benirose/O_C-BenisphereSuite) which is a fork of [Hemisphere Suite](https://github.com/Chysn/O_C-HemisphereSuite) by Jason Justian (aka chysn).

ornament**s** & crime**s** is a collaborative project by Patrick Dowling (aka pld), mxmxmx and Tim Churches (aka bennelong.bicyclist) (though mostly by pld and bennelong.bicyclist). it **(considerably) extends** the original firmware for the o_C / ASR eurorack module, designed by mxmxmx.

http://ornament-and-cri.me/
