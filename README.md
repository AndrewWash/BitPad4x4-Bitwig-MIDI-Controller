# JJ4x4 → Bitwig USB-MIDI Controller

Custom firmware that turns a **KPrepublic JJ4x4** macropad (ATmega32A, 16 keys)
into a class-compliant **USB-MIDI controller** for **Bitwig Studio** — a Drum
Machine controller with a second device-navigation mode.

this is standalone firmware, not a QMK keymap.

## Layout

```
firmware/     AVR firmware (V-USB USB-MIDI) — build with avr-gcc, flash with bootloadHID
bitwig/       Bitwig Studio JavaScript controller script
```

## Getting started

1. **Firmware** — see [`firmware/README.md`](firmware/README.md) for the
   toolchain, build (`make`), and flashing (`make flash`, hold key K11 (position 5) on
   plug-in to enter the bootloader). In plain english, download and install QMK Toolkit and do the bootloader steps, select open, open the .hex, hit flash, done.
   If you want to edit the firmware, you will need to dl QMK MSYS in order to compile it before flashing
3. **Bitwig script** — copy `bitwig/JJ4x4/` into
   `Documents\Bitwig Studio\Controller Scripts\`, (or wherever you controller scripts folder is pointing to within bitwig) then add it in Bitwig under
   *Settings → Controllers* and select the **JJ4x4 MIDI** input port.

## How it works

layout=    

| 0  | 1  | 2  | 3  |                                                            
| 4  | 5  | 6  | 7  |                                              
| 8  | 9  |10|11|                           
|12|13|14|15|

0+15=cycle modes/change pg  

Pg1: Drum machine mode  
    a. One to one with pad assignments. Use macros to navigate drum machine  
Macros in drum machine mode (pg1): also works as chromatic mode for synths/note devices  
	a. 0+3=move drum machine active pads up 1 full screen  
	b. 12+15=move drum machine active pads down 1 full screen  
	c. 12+15+9=move drum machine active pads up one ROW  
	d. 12+15+13=move drum machine active pads down one ROW  
	
Pg2: Device+ottopot companion  
	a. 0=Prev Parameter page  
	b. 3=Next parameter page  
	c. 1=collapse/expand device  
	d. 2=show/hide remote controls  
	e. 4=prev device  
	f. 6=show/hide expanded device view  
	g. 7=next device  
	h. 8=up by track (on arranger- can immedialty use device buttons)  
	i. 12=down by track (on arranger- can immedialty use device buttons)  
15=insert device at end of chain  

Full command map and CC numbers are in [`firmware/README.md`](firmware/README.md).

The original design plan is in
`~/.claude/plans/i-want-to-write-melodic-trinket.md`.
