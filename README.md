# Pilomar Pico firmware

## Firmware for driving two standard stepper drivers

You can use the board files in hardware/ to have the board fabbed, or you can use a stock pico and wire up the stepper drivers according to the schematic.

The pico will appear as a USB network device and two serial ports, the first of which is the console, which is also available on a JST-SM connector on the board.

It serves a REST API as described in the PilomarAPI.yaml file, which is maintained in the jPiLomar project.
