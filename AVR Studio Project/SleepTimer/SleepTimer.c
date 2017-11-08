/*************************************************************************
Title:    Sleep timer. Wakeup a singleboard linux PC after a specified time.
Author:   Torsten Schumacher
Software: AVR-GCC AVR-Studio 5
Hardware: ATtiny2313

DESCRIPTION:
          The SBPC communicates via UART.
		  A message "SLEEPTIME 123min:x"
		  with 123 some digits and
		  x a single digit specifying the LED status when the SBPC is turned off.
		  0:green 1:blink red
		  A message "reboot: System halted"
		  will actually shut off the SBPC.
		  When the SBPC is shut down, a button press will power it on.
		  When the SBPC is on, a button press will send "shutdown" to the UART.
		  When the SBPC is on the LED will be red.
*************************************************************************/
#include <stdlib.h>
#include <ctype.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "uart.h"
#include "key.h"

/* IO pin definitions */
#define IO_LED_RED		PB2		// OC0A pin. Can not be changed
#define IO_LED_GREEN	PD5		// OC0B pin. OC (blink) currently not used.
#define IO_BUTTON		PD3
#define IO_RELAIS		PD4
#define IO_UART_RX		PD0
#define IO_UART_TX      PD1

#define PORTB_OUTPUTS	(1<<IO_LED_RED)
#define PORTD_OUTPUTS	(1<<IO_UART_TX | 1<<IO_RELAIS | 1<<IO_LED_GREEN)

/* UART buad rate here */
#define UART_BAUD_RATE      115200      
#define F_CPU 7372899L

// 10ms system timer
// system timer frequency [Hz], one timer tick every 10ms
#define SYS_TIMER_FREQ		100			
#define SYS_TIMER_CYCLE	    ((int)(F_CPU / 256.0 / SYS_TIMER_FREQ + 0.5))
volatile uint8_t timer_flag = 0;

// strings to match
char match1[] PROGMEM = "SLEEPTIME \001min:\002";
char match2[] PROGMEM = "reboot: System halted";

// shutdown message to send.
char shutdown[] PROGMEM = "shutdown\r";

// result registers for match1
uint16_t num1;
uint8_t  num2;

// global state
uint16_t sleepMin;
uint8_t  signalMode;
uint8_t  isON;

// action indicator set by tasks
uint8_t  action;
#define ACTN_KEYPRESS   1
#define ACTN_ALARM      2
#define ACTN_SWITCH_OFF 4 

// led modes
#define LED_GREEN       0
#define LED_RED_BLINK   1
#define LED_RED         2

int matchChar1 (char c)
{
	static uint8_t count = 0;
		
	char next;
	while (next=pgm_read_byte (&(match1[count]))) {
		if (next == '\001') {    // optional digits
			if (isdigit(c)) {
				num1 = num1*10 + (c-0x30);
				next = 0;   // do not increment count. more digits may follow
			} else {
				count++;   // check if next char matches
				continue;
			}
		} else if (next == '\002') {  // single digit 0 or 1
			if (isdigit(c)) {
				num2 = c & 0x01;
				next = 0;				
				count++;
			}						
		} else if (next == c) {  // char must match
			next = 0;				
			count++;
		}
		if (next != 0) {   // mismatch. reset state.
			count = 0;
			num1 = 0;
		}
		break;		
	}
	if (pgm_read_byte (&(match1[count])) == 0x00) {  // we've reached the end
		count = 0;
		return 1;             // success
	}
	return 0;
}

int matchChar2 (char c)
{
	static uint8_t count = 0;
		
	char next=pgm_read_byte (&(match2[count]));
    if (next == c) {  // char must match
		count++;
	} else {   // mismatch. reset state.
		count = 0;
	}
	if (pgm_read_byte (&(match2[count])) == 0x00) {  // we've reached the end
		count = 0;
		return 1;             // success
	}
	return 0;
}

static inline void UART_task()
{
	unsigned int c;	
	c = uart_getc();
    if ( ! (c & UART_NO_DATA) ) {
		if (matchChar1((char) c)) {
			sleepMin = num1;
			signalMode = num2;
		}
		if (matchChar2((char) c)) {
			action |= ACTN_SWITCH_OFF;
		}										
	}
}

static inline void CLOCK_task ()
{
	static uint16_t ticks = SYS_TIMER_FREQ * 60; // ticks per sec * 60 --> every minute

	if (--ticks == 0) {
		ticks = SYS_TIMER_FREQ * 60;
    	if (sleepMin) {
	    	if (--sleepMin == 0) {
		    	action |= ACTN_ALARM;
		    }
	    }
	}
}

void do_tasks (void)
{
	if (timer_flag) {
		CLOCK_task();
		timer_flag = 0;
    	if (get_key_press( 1 << IO_BUTTON )) {
	    	action |= ACTN_KEYPRESS;
	    }
	}
	UART_task();
}

void port_init (void)
{
	DDRB = PORTB_OUTPUTS;      // red led output
	PORTB = ~(PORTB_OUTPUTS);  // all inputs with pull up
	DDRD = PORTD_OUTPUTS;      // uart, relais, green led out
	PORTD = ~(PORTD_OUTPUTS);  // all inputs with pull up
}

void timer_init (void)
{
	// timer 0 used for LED blinking (when system clock is divided by 256)
	TCCR0A = (1<<WGM00)|(1<<WGM01);		// timer mode = fast PWM
	TCCR0B = 4;							// prescaler = 1:256
	OCR0A = 220;						// red blink is short pulse 
	OCR0B = 128;                        // green blink is 50/50. Not used currently
	TIMSK = 0;							// No interrupts

	// timer 1 used as system clock in mode 4
	TCCR1A = 0;				// WGM11 = 0 WGM10 = 0
	TCCR1B = (1<<WGM12) | (1<<CS12);	 // mode 4, prescaler = 1:256
	TCCR1C = 0;
	cli();
	OCR1AH = (uint8_t) (SYS_TIMER_CYCLE / 256);
	OCR1AL = (uint8_t) (SYS_TIMER_CYCLE % 256);
	sei();
	TIMSK |= (1<<OCIE1A);
}

void set_led (uint8_t mode)
{
	// output compare off
	TCCR0A &= ~((1<<COM0A0)|(1<<COM0A1)|(1<<COM0B0)|(1<<COM0B1));
	if (mode == LED_GREEN) {
		// green on
		PORTD |= (1<<IO_LED_GREEN);
		// red off
		PORTB &= ~(1<<IO_LED_RED);		
	} else if (mode == LED_RED) {
		// red on
		PORTB |= (1<<IO_LED_RED);		
		// green off
		PORTD &= ~(1<<IO_LED_GREEN);
	} else // if (mode == LED_RED_BLINK) 
	{
		// green off
		PORTD &= ~(1<<IO_LED_GREEN);
		// output compare A enable
		TCCR0A |= (1<<COM0A0)|(1<<COM0A1);		
	}
}

void sendShutdown() 
{
	// ^d (EOT) gives fresh login prompt
	uart_putc('\004');

	// busy wait three seconds for getty to respawn
	for (int i=0; i<3*SYS_TIMER_FREQ; i++) {   // 3 * 100 * 10ms
		while(!timer_flag)
			;
		timer_flag = 0;
	}
	
	// login as shutdown user
	uart_puts_p(shutdown);	
}	

void turnOff() {
	// busy wait a second before actually switching off
	for (int i=0; i<SYS_TIMER_FREQ; i++) {   // 100 * 10ms
		while(!timer_flag)
			;
		timer_flag = 0;
	}
	if (sleepMin == 0) {     // ensure we wake up
		sleepMin = 24 * 60;  // after a day if nothing else is set
		signalMode = 1;      // and signal malfunction 	
	}
	cli();
	CLKPR = (1<<CLKPCE);
	CLKPR = 8;  // clock / 256
	TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));	 // counter off
	TCCR1B |= (1<<CS10);                         // counter on: prescaler = 1
	sei();
	set_led(signalMode);
	// relais off
	PORTD &= ~(1<<IO_RELAIS);
	isON = 0;	
}

void turnOn() {
	cli();
	CLKPR = (1<<CLKPCE);
	CLKPR = 0;  // full speed
	TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));	 // counter off
	TCCR1B |= (1<<CS12);                         // counter on: prescaler = 256
	sei();
	sleepMin = 0;
	set_led(LED_RED);
	// relais on
	PORTD |= (1<<IO_RELAIS);
	isON = 1;
}

int main(void)
{
	port_init();
    uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) );
	timer_init(); 
    
    turnOn();

    for(;;) {
		action = 0;
		do_tasks();
		if(isON) {
			if (action & ACTN_KEYPRESS) {
				sendShutdown();
			}
			if (action & ACTN_SWITCH_OFF) {
				turnOff();
			}
		} else {
			if (action & (ACTN_KEYPRESS | ACTN_ALARM)) {
				turnOn();
			}
		}			
    }    
}

/******************************
 * interrupt service routines *
 ******************************/

ISR(TIMER1_COMPA_vect)
// system timer interrupt
{
	timer_flag = 1;  // signal timer tick
	KEY_interrupt();
}


