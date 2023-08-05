# What is this?

I have a garden window in my kitchen. The top of the garden window opens up.
The window crank to open the window is... also at the top. So what do? I
really don't want to climb up onto my kitchen counters to get ventilation
in the kitchen.

Introducing...

## Eric Window Open Pro

Over the course of a long weekend I automated my window. I have a 3d printer,
an ESP8266, a simple motor controller (L298N), a few magnetic reed sensors,
a simple DC motor, and an inspiration
[YouTube video](https://www.youtube.com/watch?v=Kz8dQG2_uhw) to get started.

You put it all together and you get this...

Mostly I actually just really want to put my code somewhere in the cloud so I
don't loose it.

## Features

*   A state machine that works similarly to your garage door
    *   Push button for up, pause, close, etc...
*   Support for an open/close sensor to stop motor
*   Timeouts so you don't burn ~~your house down~~ small DC motor out 
    *   Adjustable, save settings to `E E P R O M`!
*   Web interface for control
*   Support for hardware button

## Future work ideas

I am lazy and this works for now; I can open my window from a web browser on my
local network. Good enough. My one feature request would be:

*   Integration with my smart speaker so when I inevitibly get asked if I
    closed all the windows I don't have to dig for my phone.

Also, I don't want the world to know my WiFi Password; it would be neat if that
could either be flashed to `E E P R O M` via some initial setup where the
ESP8266 broadcasts setup network (or perhaps via lazily in code even).

Thanks for coming to my ted talk.
