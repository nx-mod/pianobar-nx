#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <switch.h>

#include "nx_ui.h"

static PadState g_pad;

void BarNxUiInit (void) {
	consoleInit (NULL);

	padConfigureInput (1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault (&g_pad);
}

void BarNxUiExit (void) {
	consoleExit (NULL);
}

void BarNxUiUpdate (void) {
	consoleUpdate (NULL);
}

/* Mirrors the default keybindings in ui_dispatch.h's dispatchActions
 * table -- only the ones useful in realtime while a song is playing.
 * Everything else (search, rename, etc.) is reached through these via
 * the normal dispatch flow and uses swkbd for its own text entry. */
static char mapButtons (u64 kDown) {
	if (kDown & HidNpadButton_A) return 'p';       /* play/pause */
	if (kDown & HidNpadButton_X) return '+';       /* love */
	if (kDown & HidNpadButton_Y) return '-';       /* ban */
	if (kDown & HidNpadButton_R) return 'n';       /* next song */
	if (kDown & HidNpadButton_L) return 's';       /* change station */
	if (kDown & HidNpadButton_ZL) return '(';      /* volume down */
	if (kDown & HidNpadButton_ZR) return ')';      /* volume up */
	if (kDown & HidNpadButton_Minus) return '?';   /* help */
	if (kDown & HidNpadButton_Plus) return 'q';    /* quit */
	return 0;
}

char BarNxPollCommandKey (int timeoutSec) {
	if (timeoutSec < 0) {
		timeoutSec = 0;
	}
	/* poll at 20Hz for the duration of the requested timeout, same
	 * overall cadence the original select()-based 1s timeout gave the
	 * main loop, but responsive to input within ~50ms instead of only
	 * checking once per second */
	const int ticks = timeoutSec <= 0 ? 1 : timeoutSec * 20;

	for (int i = 0; i < ticks; i++) {
		padUpdate (&g_pad);
		char c = mapButtons (padGetButtonsDown (&g_pad));
		if (c != 0) {
			return c;
		}
		BarNxUiUpdate ();
		usleep (50 * 1000);
	}

	return 0;
}

void BarNxWaitForExit (void) {
	printf ("\n[ Press + (Start) to exit ]\n");
	BarNxUiUpdate ();

	for (;;) {
		padUpdate (&g_pad);
		if (padGetButtonsDown (&g_pad) & HidNpadButton_Plus) {
			return;
		}
		BarNxUiUpdate ();
		usleep (50 * 1000);
	}
}

size_t BarNxReadLine (char *buf, size_t bufSize, bool isPassword,
		const char *guideText) {
	SwkbdConfig kbd;
	memset (buf, 0, bufSize);

	if (R_FAILED (swkbdCreate (&kbd, 0))) {
		return 0;
	}

	if (isPassword) {
		swkbdConfigMakePresetPassword (&kbd);
	} else {
		swkbdConfigMakePresetDefault (&kbd);
	}
	if (guideText != NULL) {
		swkbdConfigSetGuideText (&kbd, guideText);
	}

	Result rc = swkbdShow (&kbd, buf, bufSize);
	swkbdClose (&kbd);

	if (R_FAILED (rc)) {
		/* user cancelled */
		buf[0] = '\0';
		return 0;
	}

	return strlen (buf);
}
