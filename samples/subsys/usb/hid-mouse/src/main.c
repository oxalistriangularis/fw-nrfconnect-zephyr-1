/*
 * Copyright (c) 2018 qianfan Zhao
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <device.h>
#include <gpio.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>

#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(main);

/* change this to use another GPIO port */
#ifdef SW0_GPIO_CONTROLLER
#define PORT0 SW0_GPIO_CONTROLLER
#else
#error SW0_GPIO_CONTROLLER needs to be set
#endif

/* change this to use another GPIO pin */
#ifdef SW0_GPIO_PIN
#define PIN0     SW0_GPIO_PIN
#else
#error SW0_GPIO_PIN needs to be set
#endif

/* The switch pin pull-up/down flags */
#ifdef SW0_GPIO_FLAGS
#define PIN0_FLAGS SW0_GPIO_FLAGS
#else
#error SW0_GPIO_FLAGS needs to be set
#endif

/* If second button exists, use it as right-click. */
#ifdef SW1_GPIO_PIN
#define PIN1	SW1_GPIO_PIN
#endif

#ifdef SW1_GPIO_CONTROLLER
#define PORT1	SW1_GPIO_CONTROLLER
#endif

#ifdef SW1_GPIO_FLAGS
#define PIN1_FLAGS SW1_GPIO_FLAGS
#endif

/* If third button exists, use it as X axis movement. */
#ifdef SW2_GPIO_PIN
#define PIN2	SW2_GPIO_PIN
#endif

#ifdef SW2_GPIO_CONTROLLER
#define PORT2	SW2_GPIO_CONTROLLER
#endif

#ifdef SW2_GPIO_FLAGS
#define PIN2_FLAGS SW2_GPIO_FLAGS
#endif

/* If fourth button exists, use it as Y axis movement. */
#ifdef SW3_GPIO_PIN
#define PIN3	SW3_GPIO_PIN
#endif

#ifdef SW3_GPIO_CONTROLLER
#define PORT3	SW3_GPIO_CONTROLLER
#endif

#ifdef SW3_GPIO_FLAGS
#define PIN3_FLAGS SW3_GPIO_FLAGS
#endif

#define LED_PORT	LED0_GPIO_CONTROLLER
#define LED		LED0_GPIO_PIN

static const u8_t hid_report_desc[] = {
	HID_GI_USAGE_PAGE, USAGE_GEN_DESKTOP,
	/* USAGE_PAGE (Generic Desktop)				05 01 */
	HID_LI_USAGE, USAGE_GEN_DESKTOP_MOUSE,
	/* USAGE (Mouse)					09 02 */
		HID_MI_COLLECTION, COLLECTION_APPLICATION,
		/* COLLECTION (Application)			A1 01 */
			HID_LI_USAGE, USAGE_GEN_DESKTOP_POINTER,
		/* 	USAGE (Pointer)				09 01 */
			HID_MI_COLLECTION, COLLECTION_PHYSICAL,
		/* 	COLLECTION (Physical)			A1 00 */
				HID_GI_USAGE_PAGE, USAGE_GEN_BUTTON,
		/* 		USAGE_PAGE (Button)		05 09 */
				HID_LI_USAGE_MIN(1), 0x01,
		/* 		USAGE_MINIMUM (Button 1)	19 01 */
				HID_LI_USAGE_MAX(1), 0x03,
		/* 		USAGE_MAXIMUM (Button 3)	29 03 */
				HID_GI_LOGICAL_MIN(1), 0,
		/* 		LOGICAL_MINIMUM (0)		15 00 */
				HID_GI_LOGICAL_MAX(1), 1,
		/* 		LOGICAL_MAXIMUM (1)		25 01 */
				HID_GI_REPORT_COUNT, 3,
		/* 		REPORT_COUNT (3)		95 03 */
				HID_GI_REPORT_SIZE, 1,
		/* 		REPORT_SIZE (1)			75 01 */
				HID_MI_INPUT, 0x02,
		/* 		INPUT (Data,Var,Abs)		81 02 */
				HID_GI_REPORT_COUNT, 1,
		/* 		REPORT_COUNT (1)		95 01 */
				HID_GI_REPORT_SIZE, 5,
		/* 		REPORT_SIZE (5)			75 05 */
				HID_MI_INPUT, 0x01,
		/* 		INPUT (Cnst,Ary,Abs)		81 01 */
				HID_GI_USAGE_PAGE, USAGE_GEN_DESKTOP,
		/* 		USAGE_PAGE (Generic Desktop)	05 01 */
				HID_LI_USAGE, USAGE_GEN_DESKTOP_X,
		/* 		USAGE (X)			09 30 */
				HID_LI_USAGE, USAGE_GEN_DESKTOP_Y,
		/* 		USAGE (Y)			09 31 */
				HID_LI_USAGE, USAGE_GEN_DESKTOP_WHEEL,
		/* 		USAGE (Wheel)			09 38 */
				HID_GI_LOGICAL_MIN(1), -127,
		/* 		LOGICAL_MINIMUM (-127)		15 81 */
				HID_GI_LOGICAL_MAX(1), 127,
		/* 		LOGICAL_MAXIMUM (127)		25 7F */
				HID_GI_REPORT_SIZE, 8,
		/* 		REPORT_SIZE (8)			75 08 */
				HID_GI_REPORT_COUNT, 3,
		/* 		REPORT_COUNT (3)		95 03 */
				HID_MI_INPUT, 0x06,
		/* 		INPUT (Data,Var,Rel)		81 06 */
			HID_MI_COLLECTION_END,
		/* 	END_COLLECTION				C0    */
		HID_MI_COLLECTION_END,
		/* END_COLLECTION				C0    */
};

static u32_t def_val[4];
static volatile u8_t status[4];
static K_SEM_DEFINE(sem, 0, 1);	/* starts off "not available" */
static struct gpio_callback callback[4];

#define MOUSE_BTN_REPORT_POS	0
#define MOUSE_X_REPORT_POS	1
#define MOUSE_Y_REPORT_POS	2

#define MOUSE_BTN_LEFT		BIT(0)
#define MOUSE_BTN_RIGHT		BIT(1)
#define MOUSE_BTN_MIDDLE	BIT(2)


static void left_button(struct device *gpio, struct gpio_callback *cb,
			u32_t pins)
{
	u32_t cur_val;
	u8_t state = status[MOUSE_BTN_REPORT_POS];

	gpio_pin_read(gpio, PIN0, &cur_val);
	if (def_val[0] != cur_val) {
		state |= MOUSE_BTN_LEFT;
	} else {
		state &= ~MOUSE_BTN_LEFT;
	}

	if (status[MOUSE_BTN_REPORT_POS] != state) {
		status[MOUSE_BTN_REPORT_POS] = state;
		k_sem_give(&sem);
	}
}

#ifdef SW1_GPIO_PIN
static void right_button(struct device *gpio, struct gpio_callback *cb,
			 u32_t pins)
{
	u32_t cur_val;
	u8_t state = status[MOUSE_BTN_REPORT_POS];

	gpio_pin_read(gpio, PIN1, &cur_val);
	if (def_val[0] != cur_val) {
		state |= MOUSE_BTN_RIGHT;
	} else {
		state &= ~MOUSE_BTN_RIGHT;
	}

	if (status[MOUSE_BTN_REPORT_POS] != state) {
		status[MOUSE_BTN_REPORT_POS] = state;
		k_sem_give(&sem);
	}
}
#endif

#ifdef SW2_GPIO_PIN
static void x_move(struct device *gpio, struct gpio_callback *cb, u32_t pins)
{
	u32_t cur_val;
	u8_t state = status[MOUSE_X_REPORT_POS];

	gpio_pin_read(gpio, PIN2, &cur_val);

	if (def_val[2] != cur_val) {
		state += 10;
	}

	if (status[MOUSE_X_REPORT_POS] != state) {
		status[MOUSE_X_REPORT_POS] = state;
		k_sem_give(&sem);
	}
}
#endif

#ifdef SW3_GPIO_PIN
static void y_move(struct device *gpio, struct gpio_callback *cb, u32_t pins)
{
	u32_t cur_val;
	u8_t state = status[MOUSE_Y_REPORT_POS];

	gpio_pin_read(gpio, PIN3, &cur_val);

	if (def_val[3] != cur_val) {
		state += 10;
	}

	if (status[MOUSE_Y_REPORT_POS] != state) {
		status[MOUSE_Y_REPORT_POS] = state;
		k_sem_give(&sem);
	}
}
#endif

int callbacks_configure(struct device *gpio, u32_t pin, int flags,
			void (*handler)(struct device*, struct gpio_callback*,
			u32_t), struct gpio_callback *callback, u32_t *val)
{
	if (!gpio) {
		LOG_ERR("Could not find PORT");
		return -ENXIO;
	}
	gpio_pin_configure(gpio, pin,
			   GPIO_DIR_IN | GPIO_INT |
			   GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE |
			   flags);
	gpio_pin_read(gpio, pin, val);
	gpio_init_callback(callback, handler, BIT(pin));
	gpio_add_callback(gpio, callback);
	gpio_pin_enable_callback(gpio, pin);
	return 0;
}

void main(void)
{
	u8_t report[4] = { 0x00 };
	u8_t toggle = 0;
	struct device *dev = device_get_binding(LED_PORT);

	gpio_pin_configure(dev, LED, GPIO_DIR_OUT);

	if (callbacks_configure(device_get_binding(PORT0), PIN0, PIN0_FLAGS,
				&left_button, &callback[0], &def_val[0])) {
		LOG_ERR("Failed configuring left button callback.");
		return;
	}

#ifdef SW1_GPIO_PIN
	if (callbacks_configure(device_get_binding(PORT1), PIN1, PIN1_FLAGS,
				&right_button, &callback[1], &def_val[1])) {
		LOG_ERR("Failed configuring right button callback.");
		return;
	}
#endif

#ifdef SW2_GPIO_PIN
	if (callbacks_configure(device_get_binding(PORT2), PIN2, PIN2_FLAGS,
				&x_move, &callback[2], &def_val[2])) {
		LOG_ERR("Failed configuring X axis movement callback.");
		return;
	}
#endif

#ifdef SW3_GPIO_PIN
	if (callbacks_configure(device_get_binding(PORT3), PIN3, PIN3_FLAGS,
				&y_move, &callback[3], &def_val[3])) {
		LOG_ERR("Failed configuring Y axis movement callback.");
		return;
	}
#endif

	usb_hid_register_device(hid_report_desc, sizeof(hid_report_desc), NULL);
	usb_hid_init();

	while (true) {
		k_sem_take(&sem, K_FOREVER);

		report[MOUSE_BTN_REPORT_POS] = status[MOUSE_BTN_REPORT_POS];
		report[MOUSE_X_REPORT_POS] = status[MOUSE_X_REPORT_POS];
		status[MOUSE_X_REPORT_POS] = 0;
		report[MOUSE_Y_REPORT_POS] = status[MOUSE_Y_REPORT_POS];
		status[MOUSE_Y_REPORT_POS] = 0;
		hid_int_ep_write(report, sizeof(report), NULL);

		/* Toggle LED on sent report */
		gpio_pin_write(dev, LED, toggle);
		toggle = !toggle;
	}
}
