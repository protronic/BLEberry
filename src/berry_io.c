/* SPDX-License-Identifier: Apache-2.0
 *
 * Native Berry functions for board I/O:
 *
 *   millis()               uptime in milliseconds
 *   reboot()               cold reboot
 *   led(on) / led(i, on)   user LEDs (led0..led2 aliases)
 *   button([i])            user buttons (sw0..sw2 aliases), 1 = pressed
 *   joy()                  joystick direction (adc-keys, input subsystem)
 *   temp()                 die temperature in degrees Celsius
 *   pinmode(p, pin, mode)  configure any GPIO, p = "a".."h"
 *   dwrite(p, pin, v)      write GPIO
 *   dread(p, pin)          read GPIO
 *   help()                 list what is available on this board
 *
 * Everything is guarded by devicetree/Kconfig, so boards without a
 * given peripheral simply don't get the corresponding function.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <ctype.h>
#include <stdio.h>

#include "berry_io.h"
#include "nus_io.h"
#include "bleberry_git.h"

#if defined(CONFIG_SENSOR) && DT_NODE_EXISTS(DT_ALIAS(die_temp0))
#define HAS_DIE_TEMP 1
#include <zephyr/drivers/sensor.h>
#endif

#if defined(CONFIG_INPUT)
#define HAS_JOYSTICK 1
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#endif

/* --- user LEDs (aliases led0..led2) ------------------------------------- */

static const struct gpio_dt_spec leds[] = {
#if DT_NODE_EXISTS(DT_ALIAS(led0))
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led1))
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
#endif
#if DT_NODE_EXISTS(DT_ALIAS(led2))
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
#endif
};

/* --- user buttons (aliases sw0..sw2) ------------------------------------- */

static const struct gpio_dt_spec buttons[] = {
#if DT_NODE_EXISTS(DT_ALIAS(sw0))
	GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
#endif
#if DT_NODE_EXISTS(DT_ALIAS(sw1))
	GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
#endif
#if DT_NODE_EXISTS(DT_ALIAS(sw2))
	GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
#endif
};

/* --- raw GPIO port access ------------------------------------------------ */

static const struct {
	char name;
	const struct device *dev;
} gpio_ports[] = {
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpioa))
	{ 'a', DEVICE_DT_GET(DT_NODELABEL(gpioa)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpiob))
	{ 'b', DEVICE_DT_GET(DT_NODELABEL(gpiob)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpioc))
	{ 'c', DEVICE_DT_GET(DT_NODELABEL(gpioc)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpiod))
	{ 'd', DEVICE_DT_GET(DT_NODELABEL(gpiod)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpioe))
	{ 'e', DEVICE_DT_GET(DT_NODELABEL(gpioe)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpiof))
	{ 'f', DEVICE_DT_GET(DT_NODELABEL(gpiof)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpiog))
	{ 'g', DEVICE_DT_GET(DT_NODELABEL(gpiog)) },
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(gpioh))
	{ 'h', DEVICE_DT_GET(DT_NODELABEL(gpioh)) },
#endif
};

/* Resolve a port argument ("a", "A", "pa", 0) to a GPIO device. */
static const struct device *port_arg(bvm *vm, int index)
{
	char name = 0;

	if (be_isint(vm, index)) {
		bint n = be_toint(vm, index);

		if (n >= 0 && n < 26) {
			name = (char)('a' + n);
		}
	} else if (be_isstring(vm, index)) {
		const char *s = be_tostring(vm, index);

		if (s[0] == 'p' || s[0] == 'P') {
			s++;
		}
		name = (char)tolower((unsigned char)s[0]);
	}

	for (size_t i = 0; i < ARRAY_SIZE(gpio_ports); i++) {
		if (gpio_ports[i].name == name) {
			return gpio_ports[i].dev;
		}
	}
	be_raise(vm, "value_error", "unknown GPIO port");
	return NULL;
}

static gpio_pin_t pin_arg(bvm *vm, int index)
{
	if (!be_isint(vm, index)) {
		be_raise(vm, "type_error", "pin must be an integer");
	}

	bint pin = be_toint(vm, index);

	if (pin < 0 || pin > 15) {
		be_raise(vm, "value_error", "pin must be 0..15");
	}
	return (gpio_pin_t)pin;
}

static bool bool_arg(bvm *vm, int index)
{
	if (be_isbool(vm, index)) {
		return be_tobool(vm, index);
	}
	if (be_isint(vm, index)) {
		return be_toint(vm, index) != 0;
	}
	be_raise(vm, "type_error", "expected bool or int");
	return false;
}

/* --- native functions ----------------------------------------------------- */

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
static int m_led(bvm *vm)
{
	int argc = be_top(vm);
	int idx = 0;
	int argi = 1;

	if (argc >= 2) {
		if (!be_isint(vm, 1)) {
			be_raise(vm, "type_error", "led index must be an integer");
		}
		idx = (int)be_toint(vm, 1);
		argi = 2;
	}
	if (argc < argi) {
		be_raise(vm, "value_error", "usage: led(on) or led(i, on)");
	}
	if (idx < 0 || idx >= (int)ARRAY_SIZE(leds)) {
		be_raise(vm, "index_error", "no such LED");
	}
	gpio_pin_set_dt(&leds[idx], bool_arg(vm, argi));
	be_return_nil(vm);
}
#endif

#if DT_NODE_EXISTS(DT_ALIAS(sw0))
static int m_button(bvm *vm)
{
	int idx = 0;

	if (be_top(vm) >= 1 && be_isint(vm, 1)) {
		idx = (int)be_toint(vm, 1);
	}
	if (idx < 0 || idx >= (int)ARRAY_SIZE(buttons)) {
		be_raise(vm, "index_error", "no such button");
	}
	be_pushint(vm, gpio_pin_get_dt(&buttons[idx]) > 0 ? 1 : 0);
	be_return(vm);
}
#endif

#ifdef HAS_JOYSTICK
static atomic_t joy_code = ATOMIC_INIT(0);

static void joy_input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_KEY) {
		return;
	}
	if (evt->value) {
		atomic_set(&joy_code, evt->code);
	} else if (atomic_get(&joy_code) == evt->code) {
		atomic_set(&joy_code, 0);
	}
}
INPUT_CALLBACK_DEFINE(NULL, joy_input_cb, NULL);

static int m_joy(bvm *vm)
{
	const char *s;

	switch (atomic_get(&joy_code)) {
	case INPUT_KEY_UP:    s = "up";    break;
	case INPUT_KEY_DOWN:  s = "down";  break;
	case INPUT_KEY_LEFT:  s = "left";  break;
	case INPUT_KEY_RIGHT: s = "right"; break;
	case INPUT_KEY_ENTER: s = "enter"; break;
	default:              s = "";      break;
	}
	be_pushstring(vm, s);
	be_return(vm);
}
#endif /* HAS_JOYSTICK */

#ifdef HAS_DIE_TEMP
static const struct device *const die_temp_dev = DEVICE_DT_GET(DT_ALIAS(die_temp0));

static int m_temp(bvm *vm)
{
	struct sensor_value val;

	if (!device_is_ready(die_temp_dev) ||
	    sensor_sample_fetch(die_temp_dev) != 0 ||
	    sensor_channel_get(die_temp_dev, SENSOR_CHAN_DIE_TEMP, &val) != 0) {
		be_raise(vm, "io_error", "die temperature sensor failed");
	}
	be_pushreal(vm, (breal)sensor_value_to_double(&val));
	be_return(vm);
}
#endif /* HAS_DIE_TEMP */

static int m_pinmode(bvm *vm)
{
	const struct device *dev = port_arg(vm, 1);
	gpio_pin_t pin = pin_arg(vm, 2);
	gpio_flags_t flags;

	if (be_top(vm) < 3 || !be_isstring(vm, 3)) {
		be_raise(vm, "type_error",
			 "mode must be \"in\", \"in_pu\", \"in_pd\", \"out\" or \"out_od\"");
	}

	const char *mode = be_tostring(vm, 3);

	if (strcmp(mode, "in") == 0) {
		flags = GPIO_INPUT;
	} else if (strcmp(mode, "in_pu") == 0) {
		flags = GPIO_INPUT | GPIO_PULL_UP;
	} else if (strcmp(mode, "in_pd") == 0) {
		flags = GPIO_INPUT | GPIO_PULL_DOWN;
	} else if (strcmp(mode, "out") == 0) {
		flags = GPIO_OUTPUT_INACTIVE;
	} else if (strcmp(mode, "out_od") == 0) {
		flags = GPIO_OUTPUT_INACTIVE | GPIO_OPEN_DRAIN;
	} else {
		be_raise(vm, "value_error", "unknown GPIO mode");
		be_return_nil(vm);
	}

	if (gpio_pin_configure(dev, pin, flags) != 0) {
		be_raise(vm, "io_error", "gpio configure failed");
	}
	be_return_nil(vm);
}

static int m_dwrite(bvm *vm)
{
	const struct device *dev = port_arg(vm, 1);
	gpio_pin_t pin = pin_arg(vm, 2);

	if (be_top(vm) < 3) {
		be_raise(vm, "value_error", "usage: dwrite(port, pin, value)");
	}
	if (gpio_pin_set_raw(dev, pin, bool_arg(vm, 3) ? 1 : 0) != 0) {
		be_raise(vm, "io_error", "gpio write failed");
	}
	be_return_nil(vm);
}

static int m_dread(bvm *vm)
{
	const struct device *dev = port_arg(vm, 1);
	gpio_pin_t pin = pin_arg(vm, 2);
	int val = gpio_pin_get_raw(dev, pin);

	if (val < 0) {
		be_raise(vm, "io_error", "gpio read failed");
	}
	be_pushint(vm, val);
	be_return(vm);
}

static int m_help(bvm *vm)
{
	be_writestring("BLEberry REPL (" CONFIG_BOARD ", git " BLEBERRY_GIT_HASH ")"
		       " - built-in functions:\n"
		       "  help()                 this help\n"
		       "  millis()               uptime in ms\n"
		       "  reboot()               cold reboot\n");
#if DT_NODE_EXISTS(DT_ALIAS(led0))
	char led_line[64];

	snprintf(led_line, sizeof(led_line),
		 "  led(on), led(i, on)    user LEDs, i = 0..%d\n",
		 (int)ARRAY_SIZE(leds) - 1);
	be_writestring(led_line);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(sw0))
	be_writestring("  button([i])            user button, 1 = pressed\n");
#endif
#ifdef HAS_JOYSTICK
	be_writestring("  joy()                  \"up\",\"down\",\"left\",\"right\",\"enter\" or \"\"\n");
#endif
#ifdef HAS_DIE_TEMP
	be_writestring("  temp()                 die temperature in deg C\n");
#endif
	be_writestring("  pinmode(p, pin, mode)  p=\"a\"..\"h\", mode: in,in_pu,in_pd,out,out_od\n"
		       "  dwrite(p, pin, v)      write GPIO pin\n"
		       "  dread(p, pin)          read GPIO pin\n"
		       "BlockBerry runtime: sps.every/wait/input/output, sensor.ready/temp,\n"
		       "  escalation.raise_if, signal.set, monitor.record, log.print\n"
		       "  channels: \"LED0\", \"SW0\", \"JOY_UP\", \"D2\"..\"D15\", \"PA0\"..\"PH15\"\n"
		       "modules: import string / math / json / gc / introspect\n"
		       "docs: https://berry-lang.github.io/\n");
	be_return_nil(vm);
}

/* --- init & registration -------------------------------------------------- */

void berry_io_init(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		if (gpio_is_ready_dt(&leds[i])) {
			gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		}
	}
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (gpio_is_ready_dt(&buttons[i])) {
			gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		}
	}
}

void berry_io_register(bvm *vm)
{
	be_regfunc(vm, "help", m_help);
	be_regfunc(vm, "millis", m_millis);
	be_regfunc(vm, "reboot", m_reboot);
#if DT_NODE_EXISTS(DT_ALIAS(led0))
	be_regfunc(vm, "led", m_led);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(sw0))
	be_regfunc(vm, "button", m_button);
#endif
#ifdef HAS_JOYSTICK
	be_regfunc(vm, "joy", m_joy);
#endif
#ifdef HAS_DIE_TEMP
	be_regfunc(vm, "temp", m_temp);
#endif
	be_regfunc(vm, "pinmode", m_pinmode);
	be_regfunc(vm, "dwrite", m_dwrite);
	be_regfunc(vm, "dread", m_dread);
}
