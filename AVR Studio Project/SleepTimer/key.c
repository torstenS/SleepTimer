#include <avr/interrupt.h>
#include "key.h"

/*********************************************************************/
/* Key debounce by Peter Dannegger                                   */
/*                                                                   */
/* See https://www.mikrocontroller.net/topic/48465                   */
/*                                                                   */
/*********************************************************************/

volatile uint8_t key_state;
volatile uint8_t key_press;

void KEY_interrupt(void)
{
	// check keys. actually we just have one
	static uint8_t ct0=0xff, ct1=0xff;
	uint8_t i;

	i = key_state ^ (~KEY_PIN);	// key changed ?
	ct0 = ~( ct0 & i );			// reset or count ct0
	ct1 = ct0 ^ (ct1 & i);		// reset or count ct1
	i &= ct0 & ct1;			    // count until roll over ?
	key_state ^= i;			    // then toggle debounced state
	key_press |= key_state & i;	// 0->1: key press detect

}

uint8_t get_key_press( uint8_t key_mask )
{
  cli();					// read and clear atomic !
  key_mask &= key_press;                        // read key(s)
  key_press ^= key_mask;                        // clear key(s)
  sei();
  return key_mask;
}

