#pragma once

#include "dsputils.h"
#include "AudioBuffer.h"
#include "AudioParam.h"
#include <Audio.h>

template <size_t BufferLength = static_cast<size_t>(AUDIO_SAMPLE_RATE),
          size_t Taps = 1>
class AudioDelayExt : public AudioStream {
public:
  AudioDelayExt() : AudioStream(1, input_queue_array) {
    delay_secs.fill(AudioParam(0.0f, 0.0002f));
  }

  void delay(size_t tap, float secs) {
    delay_secs[tap] = secs;
    if (delay_secs[tap].Read() == 0.0f)
      delay_secs[tap].Reset();
  }

  void feedback(size_t tap, float fb) {
    this->fb[tap] = fb;
  }

  void update(void) {
    auto in_block = receiveReadOnly();
    if (in_block == NULL)
      return;

    audio_block_t *outs[Taps];
    for (size_t tap = 0; tap < Taps; tap++) {
      outs[tap] = allocate();
    }

    for (size_t i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      int32_t in = in_block->data[i]; 
      for (size_t tap = 0; tap < Taps; tap++) {
        outs[tap]->data[i] = buffer.ReadInterp(delay_secs[tap].ReadNext());
        in += fb[tap].ReadNext() * outs[tap]->data[i];
      }
      buffer.WriteSample(Clip16(in));
    }
    release(in_block);
    for (size_t tap = 0; tap < Taps; tap++) {
      transmit(outs[tap], tap);
      release(outs[tap]);
    }
  }

private:
  audio_block_t *input_queue_array[1];
  std::array<AudioParam<float>, Taps> delay_secs;
  std::array<AudioParam<float>, Taps> fb;
  ExtAudioBuffer<BufferLength> buffer;
};
