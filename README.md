# Chess clock
Simple project for Microprocessors and Microcontrollers class

**How it works:**

The user selects the desired time (1min/3min/10min/30min/1h) for each player 
using *Button1* and *Button2*, then confirms the selection by pressing the *midButton*.

After confirmation, the LCD displays a pause screen with the selected times for both players. 
To start the game, one of the players presses their button, which begins the opponent’s countdown.

The clock can be paused at any time by briefly pressing the *midButton*, or reset completely 
by holding the same button for 3 seconds.

When a player’s time runs out, the display shows the winner.

The players can return to the time selection menu by holding the *midButton* for 3 seconds.

# Required components
- 1x Arduino UNO
- 1x LCD1602A (without I2C)
- 1x Breadboard
- 3x Buttons
- 3x Resistors (10kΩ)
- 1x potentiometr (10kΩ)
- 14x male-to-male jumper wires
- 11x female-male jumper wires
- (optional) 9V battery clip with DC plug (2.1 mm)

# Tinkercad Connection Diagram
![Schematic](https://github.com/nevermore19/chessClock/blob/main/Schematic.png)

# Code
The project was initially prototyped using an Arduino sketch (.ino file created by [Wiktor-Sikora](https://github.com/Wiktor-Sikora)) to verify hardware functionality. 
Once the concept was validated, the final application was rewritten in C.
