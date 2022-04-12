# arduino_lap_timer
based on the original project Personal Lap Timer by Duane B. on rcarduino.blogspot.com


This is an Arduino project.

ABOUT:
I'm an IR lap timer, you can use me to count and record lap times... eg: of your RC car model!

You can use 4 hardware normally open push buttons or an old infrared remote control with at least 4 buttons.
If your remote control have an extra button, you can use it as "fake-a-lap" feature.
If you attach a passive buzzer you can enjoy the "pressure" feature.

HARDWARE:
(mandatory)
microcontroller (atmega328)
IR receiver     (tsop31236)
display 128x64  (SSD1306 or others supported by U8X8 library)
(optional)
buzzer          (any passive buzzer)
4 buttons       (any N.O. button)
or an old infrared remote control with at least 4 buttons

USER INTERFACE:
OK = enter a submenu / change an option / start a session
CANCEL = exit a submenu / stop a session
UP = go to the next element
DOWN = go to the previous element

MENU:
main (overall summary)
-> CANCEL - memory reset -> OK/CANCEL
-> OK - start new session -> OK -> recording... -> CANCEL/FakeLAP/RealLAP
      - UP - audio -> OK to toggle
      - UP - pressure -> OK to toggle
-> UP - session summary -> OK -> lap details -> UP -> next lap of the recorded session

you have to customize getKeys() for:
- FakeLAP
you can use a fifth button on your infrared remote control or an extra hardware push button.
- RealLAP
use a different device programmed with the "simple_ir_sender" sketch or something similar.
