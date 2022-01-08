/*
 *
 * compile options.
 *
 */

#ifndef OC_OPTIONS_H_
#define OC_OPTIONS_H_

/* ------------ uncomment for Buchla/non-octaval/quartertone support: -------------------------------  */
//#define BUCHLA_SUPPORT
/* ------------ uncomment for use with Northernlight cOC program card: ------------------------------  */
//#define BUCHLA_cOC
/* ------------ uncomment for use with Northernlight 4U / cOC or 2OC: -------------------------------  */
//#define BUCHLA_4U
/* ------------ uncomment for use with Northernlight 2OC on the left side: --------------------------  */
//#define NORTHERNLIGHT_2OC_LEFTSIDE
/* ------------ print debug messages to USB serial --------------------------------------------------  */
//#define PRINT_DEBUG
/* ------------ flip screen / IO mapping ------------------------------------------------------------  */
//#define FLIP_180
/* ------------ invert screen pixels ----------------------------------------------------------------  */
//#define INVERT_DISPLAY
/* ------------ use DAC8564 -------------------------------------------------------------------------  */
//#define DAC8564


/* ------------ Hemisphere suite: Logarhythm Mod: ---------------------------------------------------  */
/* Provide a quick way to configure which full applications are included in a given build.             */
/* (Default is to disable Captain MIDI to make enough space for the additions to applets.)             */

//#define ENABLE_APP_CAPTAIN_MIDI
//#define ENABLE_APP_DARKEST_TIMELINE
//#define ENABLE_APP_PONG

// Define this to include some new, custom scales for Hemispheres Logarhythm Mod (optional if porting individual applets)
// This uses approximately 640 additional bytes of program storage space
#define HEM_LOGARHYTHM_MOD_SCALES

#endif

