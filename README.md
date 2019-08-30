# flashlight
PIC10F200 based replacement firmware for XML-T6 flashlights (Yunmai XML-T6, Elefland T6, etc) with red/white COB aux light. This flashlight can have a board marked as OR-JT42A V1OR-X5A and looks like this:

![Image of XML-T6 Flashlight](https://github.com/N-Storm/flashlight/raw/master/xml-t6-flashlight.jpg)

This firmware are intended to flash PIC10F200 and replace original MCU (unknown model) on the PCB. PIC10F200 are pin compatible with the PCB. Modes are a little different to original firmware:

1. When off: short press will turn on main light, long press will turn on COB light.
2. When on: short press will turn off the flashlight, long press will cycle mode - red/white for aux and hi-med-low for main light. That means that main light has 3 light settings implemented through simple soft PWM routine (33%/66%/100% duty cycle).

Last used mode will be saved while the battery are present. As this MCU has no EEPROM it's impossible to save mode settings without present power.

Schematics for the original board and discussion can be found here: http://forum.easyelectronics.ru/viewtopic.php?p=528268#p528268
