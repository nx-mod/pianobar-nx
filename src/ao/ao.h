/* Minimal libao-compatible shim for the Switch port.
 *
 * There's no devkitPro portlib for libao (it assumes ALSA/PulseAudio/OSS
 * etc., none of which exist here), so this reimplements just the subset of
 * the API player.c actually calls, backed by libnx's audout service.
 * audout is fixed-function hardware: 48000Hz, 2 channels (stereo), 16-bit
 * PCM -- see BarSwitchForceAudioFormat() in ao_switch.c, which is why
 * settings.c defaults sampleRate to 48000 on this port (audout can't be
 * reconfigured, so the ffmpeg filter chain has to resample to match it
 * instead).
 */
#pragma once

#include <stdint.h>

#define AO_FMT_LITTLE 1
#define AO_FMT_BIG    2
#define AO_FMT_NATIVE 4

typedef struct ao_sample_format {
	int bits;
	int rate;
	int channels;
	int byte_format;
} ao_sample_format;

typedef struct ao_option ao_option; /* unused, always NULL */
typedef struct ao_device ao_device;

void ao_initialize (void);
void ao_shutdown (void);

int ao_default_driver_id (void);
int ao_driver_id (const char *shortname);

ao_device *ao_open_live (int driver_id, ao_sample_format *format,
		ao_option *options);
ao_device *ao_open_file (int driver_id, const char *filename, int overwrite,
		ao_sample_format *format, ao_option *options);

int ao_play (ao_device *device, char *output_samples, uint32_t num_bytes);
int ao_close (ao_device *device);
