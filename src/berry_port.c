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

/* Some Berry core files (be_filelib.c, be_exec.c) reference the file
 * API even with BE_USE_FILE_SYSTEM=0. There is no file system on this
 * target, so open() fails cleanly at runtime. */

void *be_fopen(const char *filename, const char *modes)
{
	ARG_UNUSED(filename);
	ARG_UNUSED(modes);
	return NULL;
}

int be_fclose(void *hfile)
{
	ARG_UNUSED(hfile);
	return -1;
}

size_t be_fwrite(void *hfile, const void *buffer, size_t length)
{
	ARG_UNUSED(hfile);
	ARG_UNUSED(buffer);
	ARG_UNUSED(length);
	return 0;
}

size_t be_fread(void *hfile, void *buffer, size_t length)
{
	ARG_UNUSED(hfile);
	ARG_UNUSED(buffer);
	ARG_UNUSED(length);
	return 0;
}

char *be_fgets(void *hfile, void *buffer, int size)
{
	ARG_UNUSED(hfile);
	ARG_UNUSED(buffer);
	ARG_UNUSED(size);
	return NULL;
}

int be_fseek(void *hfile, long offset)
{
	ARG_UNUSED(hfile);
	ARG_UNUSED(offset);
	return -1;
}

long int be_ftell(void *hfile)
{
	ARG_UNUSED(hfile);
	return -1;
}

long int be_fflush(void *hfile)
{
	ARG_UNUSED(hfile);
	return 0;
}

size_t be_fsize(void *hfile)
{
	ARG_UNUSED(hfile);
	return 0;
}
