# RPM-To-Locked-Rotor
ATtiny85 microcontroller code to convert a PC style fan tachometer signal to a server/industrial style fan “locked rotor” signal. 

## 
RPM-To-Locked-Rotor is a simple program designed to run on an ATtiny85 microcontroller. It accepts two RPM signal inputs from PC style fans and drives two locked-rotor style open drain outputs to emulate the behavior of
server/industrial-style fans.

RPM signal inputs are on pins 7 and 2. The associated locked-rotor outputs are on pins 5 and 6. Both the inputs and outputs are intended to be pulled up externally.

This program was created to allow for the use of normal Noctua PC fans in my Eaton 9SX 2000 and 9SX 1000 UPS since they are much quieter. Without this signal conversion, the UPS will throw a fan fault/alarm. While there may be other methods for suppressing the fan fault/alarm (like using a capacitor or perhaps grounding out the signal), this program allows for the fan fault detection logic to remain fully functional.

# Instructions

* Acquire an ATtiny programmer (I used the Tiny AVR Programmer from SparkFun)
* Install an AVR toolchain (`sudo apt install gcc-avr avrdude` if using Ubuntu).
* Run `make` to build the application, then `make flash` to program the chip.
#
I ended up making a small PCB for this, but it can be easily done using a protoboard.

Basic BOM:

* Two 10uF caps (50YXM10MEFR5X11).
* One 0.1uF cap (FA28X8R1E104KNU06).
* One small 5V regulator (L4931CZ50-AP, etc.).
* Some 10K pull-up resistors (for the inputs and reset pin; the outputs are pulled up inside the UPS in my case).
* ATTINY85V-10PU
