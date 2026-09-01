/* libao shim backed by libnx's audren wrapper (switch/audio/driver.h).
 *
 * Modeled directly on TriPlayer's Sysmodule/source/nx/Audio.cpp (proven
 * working there already) rather than the simpler blocking audout API:
 * audren's voice abstraction accepts arbitrary sample rate/channel count
 * per voice and mixes down to the final sink itself, so -- unlike raw
 * audout, which is fixed at 48kHz/stereo -- pianobar doesn't need to be
 * forced to resample everything before handing it to us.
 *
 * ao_initialize()/ao_shutdown() own the renderer + voice-0 + the wave
 * buffer ring for the whole process lifetime (mirrors TriPlayer's
 * singleton Audio object). ao_open_live()/ao_close() just (re)configure
 * that single voice per song, since player.c opens a fresh "device" for
 * every track.
 */
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "ao.h"

#define AO_BUFFER_SIZE	0xC800	/* 50KB per slot, matches TriPlayer */
#define AO_MAX_BUFFERS	6
#define AO_OUT_CHANNELS	2	/* final mix is always stereo */

static const size_t realSize =
		(AO_BUFFER_SIZE + (AUDREN_MEMPOOL_ALIGNMENT - 1)) &
		~(AUDREN_MEMPOOL_ALIGNMENT - 1);

static AudioDriver g_drv;
static AudioDriverWaveBuf g_waveBuf[AO_MAX_BUFFERS];
static uint8_t *g_memPool[AO_MAX_BUFFERS];
static int g_nextBuf = 0;
static int g_sink = -1;
static int g_voice = -1;
static bool g_ready = false;

struct ao_device {
	int channels;
};

void ao_initialize (void) {
	Result rc;
	const AudioRendererConfig audrenCfg = {
		.output_rate     = AudioRendererOutputRate_48kHz,
		.num_voices      = 4,
		.num_effects     = 0,
		.num_sinks       = 1,
		.num_mix_objs    = 1,
		.num_mix_buffers = 2,
	};

	rc = audrenInitialize (&audrenCfg);
	if (R_FAILED (rc)) {
		return;
	}

	rc = audrvCreate (&g_drv, &audrenCfg, AO_OUT_CHANNELS);
	if (R_FAILED (rc)) {
		audrenExit ();
		return;
	}

	for (int i = 0; i < AO_MAX_BUFFERS; i++) {
		g_memPool[i] = aligned_alloc (AUDREN_MEMPOOL_ALIGNMENT, realSize);
		if (g_memPool[i] == NULL) {
			for (int j = 0; j < i; j++) {
				free (g_memPool[j]);
			}
			audrvClose (&g_drv);
			audrenExit ();
			return;
		}
		int id = audrvMemPoolAdd (&g_drv, g_memPool[i], realSize);
		audrvMemPoolAttach (&g_drv, id);
	}

	const uint8_t sinkChannels[AO_OUT_CHANNELS] = { 0, 1 };
	g_sink = audrvDeviceSinkAdd (&g_drv, AUDREN_DEFAULT_DEVICE_NAME,
			AO_OUT_CHANNELS, sinkChannels);
	audrvUpdate (&g_drv);
	audrenStartAudioRenderer ();

	g_ready = true;
}

void ao_shutdown (void) {
	if (!g_ready) {
		return;
	}
	if (g_voice >= 0) {
		audrvVoiceStop (&g_drv, g_voice);
		audrvVoiceDrop (&g_drv, g_voice);
		g_voice = -1;
	}
	for (int i = 0; i < AO_MAX_BUFFERS; i++) {
		free (g_memPool[i]);
	}
	audrvClose (&g_drv);
	audrenStopAudioRenderer ();
	audrenExit ();
	g_ready = false;
}

int ao_default_driver_id (void) {
	return 0;
}

int ao_driver_id (const char *shortname) {
	(void) shortname;
	return 0;
}

ao_device *ao_open_live (int driver_id, ao_sample_format *format,
		ao_option *options) {
	(void) driver_id;
	(void) options;

	if (!g_ready || format->bits != 16) {
		return NULL;
	}

	if (g_voice >= 0) {
		audrvVoiceStop (&g_drv, g_voice);
		audrvVoiceDrop (&g_drv, g_voice);
		g_voice = -1;
	}

	g_voice = 0;
	if (!audrvVoiceInit (&g_drv, g_voice, format->channels, PcmFormat_Int16,
			format->rate)) {
		g_voice = -1;
		return NULL;
	}

	audrvVoiceSetDestinationMix (&g_drv, g_voice, AUDREN_FINAL_MIX_ID);
	if (format->channels == 1) {
		audrvVoiceSetMixFactor (&g_drv, g_voice, 1.0f, 0, 0);
		audrvVoiceSetMixFactor (&g_drv, g_voice, 1.0f, 0, 1);
	} else {
		audrvVoiceSetMixFactor (&g_drv, g_voice, 1.0f, 0, 0);
		audrvVoiceSetMixFactor (&g_drv, g_voice, 0.0f, 0, 1);
		audrvVoiceSetMixFactor (&g_drv, g_voice, 0.0f, 1, 0);
		audrvVoiceSetMixFactor (&g_drv, g_voice, 1.0f, 1, 1);
	}

	for (int i = 0; i < AO_MAX_BUFFERS; i++) {
		g_waveBuf[i].state = AudioDriverWaveBufState_Done;
	}
	g_nextBuf = 0;
	audrvUpdate (&g_drv);

	ao_device *dev = malloc (sizeof (*dev));
	if (dev != NULL) {
		dev->channels = format->channels;
	}
	return dev;
}

ao_device *ao_open_file (int driver_id, const char *filename, int overwrite,
		ao_sample_format *format, ao_option *options) {
	(void) driver_id;
	(void) filename;
	(void) overwrite;
	(void) format;
	(void) options;
	/* no FIFO/pipe concept on Switch -- audioPipe isn't supported here */
	return NULL;
}

int ao_play (ao_device *device, char *output_samples, uint32_t num_bytes) {
	if (device == NULL || g_voice < 0) {
		return 0;
	}

	size_t remaining = num_bytes;
	const char *src = output_samples;

	while (remaining > 0) {
		size_t chunk = remaining < realSize ? remaining : realSize;

		/* wait for the slot we're about to reuse to free up */
		while (g_waveBuf[g_nextBuf].state != AudioDriverWaveBufState_Done &&
				g_waveBuf[g_nextBuf].state != AudioDriverWaveBufState_Free) {
			audrvUpdate (&g_drv);
			audrenWaitFrame ();
		}

		memcpy (g_memPool[g_nextBuf], src, chunk);
		armDCacheFlush (g_memPool[g_nextBuf], chunk);

		g_waveBuf[g_nextBuf].data_raw = g_memPool[g_nextBuf];
		g_waveBuf[g_nextBuf].size = chunk;
		g_waveBuf[g_nextBuf].start_sample_offset = 0;
		g_waveBuf[g_nextBuf].end_sample_offset =
				chunk / (2 * device->channels);
		audrvVoiceAddWaveBuf (&g_drv, g_voice, &g_waveBuf[g_nextBuf]);

		g_nextBuf = (g_nextBuf + 1) % AO_MAX_BUFFERS;

		if (!audrvVoiceIsPlaying (&g_drv, g_voice)) {
			audrvVoiceStart (&g_drv, g_voice);
		}
		audrvUpdate (&g_drv);

		src += chunk;
		remaining -= chunk;
	}

	return 1;
}

int ao_close (ao_device *device) {
	if (device == NULL) {
		return 0;
	}
	if (g_voice >= 0) {
		audrvVoiceStop (&g_drv, g_voice);
		audrvUpdate (&g_drv);
	}
	free (device);
	return 1;
}
