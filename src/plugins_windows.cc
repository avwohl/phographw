/*
 * plugins_windows.cc
 *
 * Stub implementations of the SoundPlugin and MIDIPlugin interfaces
 * for the Windows port of Phograph.
 *
 * The original plugins use Apple CoreAudio and CoreMIDI.
 * These stubs can be filled in with WASAPI (for sound) and
 * Windows MIDI APIs (for MIDI) in a future iteration.
 */

#include "plugins/SoundPlugin.h"
#include "plugins/MIDIPlugin.h"

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Sound plugin stubs (replace with WASAPI implementation)
// ---------------------------------------------------------------------------

extern "C" {

bool soundInit(int sampleRate, bool stereo) {
    (void)sampleRate;
    (void)stereo;
    return true;
}

void soundStop(void) {
}

int soundAvailableSpace(void) {
    return 4096;
}

int soundPlaySamples(const void* samples, int startIndex, int count) {
    (void)samples;
    (void)startIndex;
    return count;
}

int soundPlaySilence(int count) {
    return count;
}

float soundGetVolume(void) {
    return 1.0f;
}

void soundSetVolume(float volume) {
    (void)volume;
}

bool soundIsRunning(void) {
    return false;
}

// ---------------------------------------------------------------------------
// MIDI plugin stubs (replace with Windows MIDI API implementation)
// ---------------------------------------------------------------------------

bool midiInit(void) {
    return true;
}

int midiGetPortCount(void) {
    return 0;
}

char* midiGetPortName(int portIndex) {
    (void)portIndex;
    return nullptr;
}

int midiOpenPort(int portIndex) {
    (void)portIndex;
    return -1;
}

void midiClosePort(int handle) {
    (void)handle;
}

int midiRead(int handle, uint8_t* buf, int bufSize) {
    (void)handle;
    (void)buf;
    (void)bufSize;
    return 0;
}

int midiWrite(int handle, const uint8_t* data, int count) {
    (void)handle;
    (void)data;
    (void)count;
    return 0;
}

int64_t midiGetClock(void) {
    return 0;
}

bool midiSendShort(int handle, uint8_t status, uint8_t data1, uint8_t data2) {
    (void)handle;
    (void)status;
    (void)data1;
    (void)data2;
    return false;
}

bool midiSendShort2(int handle, uint8_t status, uint8_t data1) {
    (void)handle;
    (void)status;
    (void)data1;
    return false;
}

bool midiSendSysEx(int handle, const uint8_t* data, int count) {
    (void)handle;
    (void)data;
    (void)count;
    return false;
}

} // extern "C"
