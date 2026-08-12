/* SPDX-License-Identifier: Apache-2.0
 *
 * Berry REPL thread: owns the Berry VM and runs be_repl() forever,
 * reading lines from and writing results to the BLE NUS terminal.
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "berry.h"
#include "be_repl.h"
#include "nus_io.h"

LOG_MODULE_REGISTER(berry_repl, CONFIG_LOG_DEFAULT_LEVEL);

#define BANNER \
	"BLEberry - Berry " BERRY_VERSION " on Zephyr (" CONFIG_BOARD ")\n" \
	"type berry code, e.g.: 1+2  or  print(\"hi\")\n"

static char line_buf[CONFIG_BLEBERRY_LINE_MAX];

/* --- native functions ------------------------------------------------- */

static int m_millis(bvm *vm)
{
	be_pushint(vm, (bint)k_uptime_get_32());
	be_return(vm);
}

static int m_reboot(bvm *vm)
{
	(void)vm;
	nus_io_flush();
	k_sleep(K_MSEC(100));
	sys_reboot(SYS_REBOOT_COLD);
	be_return_nil(vm);
}

#if DT_NODE_EXISTS(DT_ALIAS(led0))
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static int m_led(bvm *vm)
{
	if (be_top(vm) >= 1 && (be_isbool(vm, 1) || be_isint(vm, 1))) {
		int on = be_isbool(vm, 1) ? be_tobool(vm, 1)
					  : (be_toint(vm, 1) != 0);

		gpio_pin_set_dt(&led, on);
	}
	be_return_nil(vm);
}
#endif

static void register_natives(bvm *vm)
{
	be_regfunc(vm, "millis", m_millis);
	be_regfunc(vm, "reboot", m_reboot);
#if DT_NODE_EXISTS(DT_ALIAS(led0))
	be_regfunc(vm, "led", m_led);
#endif
}

/* --- REPL glue --------------------------------------------------------- */

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

#if DT_NODE_EXISTS(DT_ALIAS(led0))
	if (gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}
#endif

	for (;;) {
		bvm *vm = be_vm_new();

		if (vm == NULL) {
			LOG_ERR("Berry VM allocation failed");
			k_sleep(K_SECONDS(5));
			continue;
		}
		register_natives(vm);

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
