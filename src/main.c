/* Copyright (C) 2024 Jacob Moroni (opensource@jakemoroni.com)
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "platform.h"
#include "jiffies.h"

/* Amount of time to wait after power on with the output
 * floating high before driving the output low and waiting
 * for the fan to spin up.
 * This is to handle the off chance that some device is
 * counting on the start-up delay of the San Ace fan to
 * check connectivity, etc.
 * NOTE: After testing the original fan, I found that
 *       it drives the signal low after ~1.2 microseconds,
 *       so set this to 0.
 */
#define POWER_ON_JIFFIES         0

/* State changes are accumulated for this period of time. */
#define SAMPLE_JIFFIES           JIFFIES_PER_SECOND

/* Amount of time to wait while driving the output low
 * before using the input RPM to determine the output state.
 * The intent is to allow the fans time to spin up
 * NOTE: Must be >= 1 multiple of SAMPLE_JIFFIES.
 */
#define SPIN_UP_JIFFIES          (SAMPLE_JIFFIES * 5u) /* 5 seconds */

/* There are two cycles per revolution, so 4 state changes.
 * For 780 RPM, this means 13 cycles per second, so
 * 52 state changes per second.
 */
#define TOGGLE_COUNT_THRESHOLD   40 /* 600 RPM */
//#define TOGGLE_COUNT_THRESHOLD   52 /* 780 RPM */
//#define TOGGLE_COUNT_THRESHOLD   64 /* 960 RPM */

/* AVR pins. */
#define NUM_CHANNELS             2
#define DDR_OUT_CHANNEL_0        DDRB
#define DDR_OUT_CHANNEL_1        DDRB
#define PIN_OUT_CHANNEL_0        PINB0
#define PIN_OUT_CHANNEL_1        PINB1
#define PIN_IN_PORT_CHANNEL_0    PINB
#define PIN_IN_PORT_CHANNEL_1    PINB
#define PIN_IN_CHANNEL_0         PINB2
#define PIN_IN_CHANNEL_1         PINB3

enum fsm_state {
	FSM_STATE_INIT,

	/* Waiting up to 50 milliseconds with the output high/floating
	 * to simulate the start-up time specified by San Ace.
	 * See: https://products.sanyodenki.com/info/sanace/en/technical_material/dcsensor.html
	 */
	FSM_STATE_POWER_ON,

	/* Waiting 5 seconds for the fans to settle before using their RPM
	 * to determine the locked rotor output state.
	 * Output is driven low at this time (no locked rotor).
	 */
	FSM_STATE_SPIN_UP,

	/* Fan RPM is used for locked rotor output control. */
	FSM_STATE_RUNNING,
};

/* Top level instance. */
struct rpm_to_locked_rotor_app {
	struct rpm_to_locked_rotor_channel {
		enum fsm_state state;
		jiffies_t time;
		jiffies_t prev_sample_time;
		bool under_threshold; /* From previous sample window. */
		bool prev_input_state;
		uint32_t toggles;
	} channels[NUM_CHANNELS];
};

/* Single instance. */
static struct rpm_to_locked_rotor_app g_inst;

/* Reads the input pin state. */
static bool read_input(uint8_t channel)
{
	if (channel) {
		return PIN_IN_PORT_CHANNEL_1 & (1u << PIN_IN_CHANNEL_1);
	}
	return PIN_IN_PORT_CHANNEL_0 & (1u << PIN_IN_CHANNEL_0);
}

/* Set pin to Hi-Z. */
static void float_high(uint8_t channel)
{
	if (channel) {
		DDR_OUT_CHANNEL_1 &= ~(1u << PIN_OUT_CHANNEL_1);
	} else {
		DDR_OUT_CHANNEL_0 &= ~(1u << PIN_OUT_CHANNEL_0);
	}
}

/* Drive the output pin low. */
static void drive_low(uint8_t channel)
{
	if (channel) {
		DDR_OUT_CHANNEL_1 |= (1u << PIN_OUT_CHANNEL_1);
	} else {
		DDR_OUT_CHANNEL_0 |= (1u << PIN_OUT_CHANNEL_0);
	}
}

/* FSM. */
static void fsm_run(struct rpm_to_locked_rotor_app *inst)
{
	size_t i;
	jiffies_t curr_time;
	bool curr_input;
	struct rpm_to_locked_rotor_channel *channel;

	curr_time = jiffies();

	for (i = 0; i < NUM_CHANNELS; i++) {
		channel = &inst->channels[i];

		switch (channel->state) {
		case FSM_STATE_INIT:
			channel->time = curr_time;
			/* Output is floating high already. */
			channel->state = FSM_STATE_POWER_ON;
			break;
		case FSM_STATE_POWER_ON:
			if ((curr_time - channel->time) > POWER_ON_JIFFIES) {
				drive_low(i);
				channel->state = FSM_STATE_SPIN_UP;
				channel->time = curr_time;
				channel->prev_sample_time = curr_time;
			}
			break;
		case FSM_STATE_SPIN_UP:
			curr_input = read_input(i);
			if (curr_input != channel->prev_input_state) {
				channel->toggles++;
				channel->prev_input_state = curr_input;
			}

			if ((curr_time - channel->prev_sample_time) > SAMPLE_JIFFIES) {
				if (channel->toggles >= TOGGLE_COUNT_THRESHOLD) {
					channel->under_threshold = false;
				} else {
					channel->under_threshold = true;
				}
				channel->toggles = 0;
				channel->prev_sample_time = curr_time;
			}

			if ((curr_time - channel->time) > SPIN_UP_JIFFIES) {
				/* We can start driving the output now. */
				channel->state = FSM_STATE_RUNNING;
			}
			break;
		case FSM_STATE_RUNNING:
			curr_input = read_input(i);
			if (curr_input != channel->prev_input_state) {
				channel->toggles++;
				channel->prev_input_state = curr_input;
			}

			if ((curr_time - channel->prev_sample_time) > SAMPLE_JIFFIES) {
				if (channel->toggles >= TOGGLE_COUNT_THRESHOLD) {
					channel->under_threshold = false;
				} else {
					channel->under_threshold = true;
				}
				channel->toggles = 0;
				channel->prev_sample_time = curr_time;
			}

			/* Drive the output pin. Float high == rotor locked. */
			if (channel->under_threshold) {
				float_high(i);
			} else {
				drive_low(i);
			}
			break;
		default:
			/* Might as well signal a fan failure. */
			float_high(i);
			break;
		}
	}
}

int main(void)
{
	struct rpm_to_locked_rotor_app *inst = &g_inst;

	/* Global pull-up disable. */
	MCUCR |= PUD;

	/* Enable interrupts. */
	sei();

	jiffies_init();

	/* Main FSM loop. */
	while (1) {
		fsm_run(inst);
	}
}
