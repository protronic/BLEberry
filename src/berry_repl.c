/* SPDX-License-Identifier: Apache-2.0
 *
 * Berry REPL thread: owns the Berry VM and runs be_repl() forever,
 * reading lines from and writing results to the BLE NUS terminal.
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "berry.h"
#include "be_repl.h"
#include "berry_io.h"
#include "nus_io.h"

LOG_MODULE_REGISTER(berry_repl, CONFIG_LOG_DEFAULT_LEVEL);

#define BANNER \
	"BLEberry - Berry " BERRY_VERSION " on Zephyr (" CONFIG_BOARD ")\n" \
	"type berry code (e.g. 1+2) or help()\n"

static char line_buf[CONFIG_BLEBERRY_LINE_MAX];

static char *repl_getline(const char *prompt)
{
	be_writestring(prompt);
	nus_io_flush();
	return nus_io_read_line(line_buf, sizeof(line_buf));
}

const char *bleberry_banner(void)
{
	return BANNER "> ";
}

static void repl_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	berry_io_init();

	for (;;) {
		bvm *vm = be_vm_new();

		if (vm == NULL) {
			LOG_ERR("Berry VM allocation failed");
			k_sleep(K_SECONDS(5));
			continue;
		}
		berry_io_register(vm);

		be_writestring(BANNER);

		int res = be_repl(vm, repl_getline, NULL);

		if (res == BE_MALLOC_FAIL) {
			be_writestring("!! out of memory, restarting VM\n");
		} else {
			be_writestring("REPL terminated, restarting VM\n");
		}
		nus_io_flush();
		be_vm_delete(vm);
	}
}

K_THREAD_DEFINE(bleberry_repl, CONFIG_BLEBERRY_REPL_STACK_SIZE,
		repl_thread, NULL, NULL, NULL,
		K_PRIO_PREEMPT(10), 0, 0);
