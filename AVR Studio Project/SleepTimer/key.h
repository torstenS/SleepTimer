#ifndef KEY_H
#define KEY_H

#include <inttypes.h>
#include <avr/io.h>

// push button
#define KEY_PIN				PIND			// Port input register where push button is connected

extern void KEY_interrupt(void);
extern uint8_t get_key_press( uint8_t key_mask);

#endif