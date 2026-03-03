/*
	Copyright 2017-2018 Arvid Brodin	arvidb@kth.se
	Modified 2026 for VESC Firmware v7.00+

	This file is meant to be compiled as part of Benjamin Vedder's
	VESC firmware.

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <stdio.h>
#include "mc_interface.h"
#include "timeout.h"
#include "terminal.h"
#include "commands.h"

// Default configuration values
// Target generator rpm (applies in both directions, always positive)
static float gen_erpm = 2000.0;

// Generator current (amperes) at target rpm (always positive)
static float gen_current = 20.0;

// At what ratio of gen_erpm to start generation.
// gen_erpm = 2000 and gen_start = 0.90 would start regenerative braking at
// 0.90 * 2000 = 1800 rpm, and will linearly increase current so that
// gen_current is reached at gen_erpm.
// (VESC_Tool limits, i.e. max motor currents & max battery current, will be
// respected.)
static float gen_start = 0.90;

static int gen_update_rate_hz = 1000;

static volatile bool stop_now = true;
static volatile bool is_running = false;

// Threads
static THD_FUNCTION(gen_thread, arg);
static THD_WORKING_AREA(gen_thread_wa, 1024);

// Terminal callbacks
static void terminal_gen_config(int argc, const char **argv);
static void terminal_gen_status(int argc, const char **argv);


void app_custom_start(void) {
	// Register terminal commands
	terminal_register_command_callback(
		"gen_config",
		"Configure generator parameters",
		"[erpm] [current] [start] [rate_hz]",
		terminal_gen_config);
	
	terminal_register_command_callback(
		"gen_status",
		"Show generator status",
		"",
		terminal_gen_status);
	
	stop_now = false;
	chThdCreateStatic(gen_thread_wa, sizeof(gen_thread_wa), NORMALPRIO, gen_thread, NULL);
}

void app_custom_stop(void) {
	terminal_unregister_callback(terminal_gen_config);
	terminal_unregister_callback(terminal_gen_status);
	
	stop_now = true;
	while (is_running) {
		chThdSleepMilliseconds(1);
	}
}

void app_custom_configure(app_configuration *conf) {
	(void) conf;
}

static THD_FUNCTION(gen_thread, arg) {
	(void)arg;

	chRegSetThreadName("App Generator");

	is_running = true;

	for(;;) {
		const float rpm_now = mc_interface_get_rpm();

		// Get speed normalized to set rpm
		const float rpm_rel = fabsf(rpm_now) / gen_erpm;

		// Start generation at gen_start * set rpm
		float current = rpm_rel - gen_start;
		if (current < 0.0)
			current = 0.0;

		// Reach 100 % of set current at set rpm
		current /= 1.00 - gen_start;

		current *= gen_current;

		if (rpm_now < 0.0) {
			mc_interface_set_current(current);
		} else {
			mc_interface_set_current(-current);
		}


		// Sleep for a time according to the specified rate
		systime_t sleep_time = CH_CFG_ST_FREQUENCY / gen_update_rate_hz;

		// At least one tick should be slept to not block the other threads
		if (sleep_time == 0) {
			sleep_time = 1;
		}
		chThdSleep(sleep_time);

		if (stop_now) {
			is_running = false;
			return;
		}

		// Reset timeout
		timeout_reset();
	}
}

// Terminal command: Configure generator parameters
static void terminal_gen_config(int argc, const char **argv) {
	if (argc == 5) {
		// Set all parameters
		sscanf(argv[1], "%f", &gen_erpm);
		sscanf(argv[2], "%f", &gen_current);
		sscanf(argv[3], "%f", &gen_start);
		sscanf(argv[4], "%d", &gen_update_rate_hz);
		
		commands_printf("Generator configuration updated:");
		commands_printf("  Target ERPM:    %.1f", (double)gen_erpm);
		commands_printf("  Max Current:    %.1f A", (double)gen_current);
		commands_printf("  Start Ratio:    %.2f (%.1f ERPM)", 
			(double)gen_start, (double)(gen_start * gen_erpm));
		commands_printf("  Update Rate:    %d Hz", gen_update_rate_hz);
	} else if (argc == 1) {
		// Display current settings
		commands_printf("Generator Configuration:");
		commands_printf("  Target ERPM:    %.1f", (double)gen_erpm);
		commands_printf("  Max Current:    %.1f A", (double)gen_current);
		commands_printf("  Start Ratio:    %.2f (%.1f ERPM)", 
			(double)gen_start, (double)(gen_start * gen_erpm));
		commands_printf("  Update Rate:    %d Hz", gen_update_rate_hz);
		commands_printf(" ");
		commands_printf("Usage: gen_config <erpm> <current> <start> <rate_hz>");
		commands_printf("Example: gen_config 2500 15 0.85 1000");
	} else {
		commands_printf("Usage: gen_config [erpm] [current] [start] [rate_hz]");
		commands_printf("Call without parameters to show current settings");
	}
}

// Terminal command: Show generator status
static void terminal_gen_status(int argc, const char **argv) {
	(void)argc; (void)argv;
	
	const float rpm_now = mc_interface_get_rpm();
	const float rpm_abs = fabsf(rpm_now);
	const float start_rpm = gen_start * gen_erpm;
	const float rpm_rel = rpm_abs / gen_erpm;
	
	// Calculate current output
	float current = rpm_rel - gen_start;
	if (current < 0.0) current = 0.0;
	current /= 1.00 - gen_start;
	current *= gen_current;
	
	commands_printf("Generator Status:");
	commands_printf("  Current RPM:    %.1f %s", (double)rpm_abs, rpm_now < 0 ? "(reverse)" : "(forward)");
	commands_printf("  Target ERPM:    %.1f", (double)gen_erpm);
	commands_printf("  Start ERPM:     %.1f", (double)start_rpm);
	commands_printf("  Active:         %s", rpm_abs >= start_rpm ? "YES" : "NO");
	commands_printf("  Gen Current:    %.1f A", (double)current);
	commands_printf("  Running:        %s", is_running ? "YES" : "NO");
}
