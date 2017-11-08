# SleepTimer
Sleep timer for RasPi and Co.

A little circuit allows to turn off a RasPi completely and turn it on after a specified time.

The circuit is no more than a minimalistic ATtiny 2313 microcontroller
and a relais that switches the RasPi power. A bicolour LED and a button 
allow for some user interaction.

RasPi and ATtiny communicat via the RasPi serial console.

- A message "SLEEPTIME 123min:x"
  - with 123 some digits and
  - x a single digit specifying the LED status when the RasPi is turned off.
  - 0:green 1:blink red

- A message "reboot: System halted" 
  - will actually shut off the Raspi.
  
When the RasPi is shut down, a button press will power it on.

When the RasPi is on, a button press will send "shutdown" to the UART.

When the RasPi is on the LED will be red.

