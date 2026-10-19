#pragma once

/**
 * Required Aliases:
 * aliases {
 *     modem = &usart1;
 *     bt-reset = &bt_reset;
 *     bt-sys-ctrl = &bt_sys_ctrl;
 * };
 *
 * Example Target Nodes:
 * &usart1 {
 *     current-speed = <115200>;
 *     status = "okay";
 * };
 *
 * bt_pins {
 *     compatible = "gpio-leds";
 *     bt_reset: bt_reset {
 *         gpios = <&gpioc 10 GPIO_ACTIVE_LOW>;
 *     };
 *     bt_sys_ctrl: bt_sys_ctrl {
 *         gpios = <&gpioc 12 GPIO_ACTIVE_HIGH>;
 *     };
 * };
 */

void threadBT(void* arg1, void* arg2, void* arg3);