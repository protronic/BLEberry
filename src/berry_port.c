/* SPDX-License-Identifier: Apache-2.0
 *
 * Berry OS port layer: standard output and input are routed to the
 * BLE NUS terminal, mirrored to the Zephyr console for debugging.
 *
 * With BE_USE_FILE_SYSTEM=0 and the os/sys/time modules disabled,
 * these two functions are the whole port surface Berry requires.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "berry.h"
#include "nus_io.h"

BERRY_API void be_writebuffer(const char *buffer, size_t length)
{
	printk("%.*s", (int)length, buffer);
	nus_io_write(buffer, length);
}

BERRY_API char *be_readstring(char *buffer, size_t size)
{
	/* fgets-like semantics for Berry's input(): return the line
	 * with a trailing newline appended. */
	nus_io_flush();
	nus_io_read_line(buffer, size - 1);

	size_t len = strlen(buffer);

	buffer[len] = '\n';
	buffer[len + 1] = '\0';
	return buffer;
}
