/* Switch-native replacement for pianobar's terminal I/O.
 *
 * pianobar's UI model is entirely terminal-shaped: printf for output,
 * blocking single-char reads for playback commands, blocking line reads
 * for text entry. None of that has anything to back it on Switch (no tty,
 * no physical keyboard) -- this gives it real backing instead of trying
 * to emulate a terminal: a libnx console for output, HidNpad polling
 * mapped to the same default command characters pianobar already uses
 * (see ui_dispatch.h's dispatchActions table) for realtime playback
 * commands, and the swkbd applet for anything that needs actual text
 * (login, search, station numbers, y/n confirmation).
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>

void BarNxUiInit (void);
void BarNxUiExit (void);

/* Flushes any buffered console output to the screen. Cheap to call
 * often -- pianobar already fflush(stdout)'s after most prints, so this
 * is called from the same places (see ui.c/ui_readline.c). */
void BarNxUiUpdate (void);

/* Polls HidNpad for up to `timeoutSec` seconds (fractional latency is
 * fine, this doesn't need to be exact -- BarMainHandleUserInput just
 * wants "check for a command, otherwise don't busy-spin the loop").
 * Returns the mapped default-keybinding character for the button
 * pressed, or 0 if nothing was pressed before the timeout. */
char BarNxPollCommandKey (int timeoutSec);

/* Shows the swkbd applet and returns the typed string (already
 * null-terminated, truncated to bufSize-1). isPassword masks input.
 * Returns the string length, or 0 if the user cancelled -- matching
 * BarReadline's own return contract. */
size_t BarNxReadLine (char *buf, size_t bufSize, bool isPassword,
		const char *guideText);

/* Prints a message and blocks until Start (+) is pressed. Whatever was
 * already on screen (an error message, etc.) stays visible -- this is
 * meant to be called right before exiting so the console doesn't vanish
 * before anyone's had a chance to read it. */
void BarNxWaitForExit (void);
