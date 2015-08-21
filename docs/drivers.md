Doodle3D print3D drivers
==============================

## Marlin driver

### Connect mode
The driver attempts to make a serial connection with an `115200` baudrate.
Every 2 seconds it'll check the temperature (see Temperature check) to determine if a connection has been made.
If after 2 temperature check attempts it didn't receive a temperature it will switch baudrates. From 115200 to 250000 or back again. It will keep doing this.
If it receives a temperature it will go into `idle` mode.

### Idle mode
Every 1,5 seconds it will perform a temperature check.

### Printing mode
Buffered gcode lines are send, line by line.
When a `Resend:{linenumber}` is received the same line is send again.
When an `ok` is received the next line is send.
Every 5 seconds it will perform a temperature check.

We'll skip gcode lines like:
- Empty lines
- Comment only lines `;{comment}`, for example `;TYPE:SKIRT`.

Example gcode:
```
M109 S210.000000
G28 X0 Y0
G0 F9000 X65.315 Y89.147 Z0.300
G1 F1200 X66.233 Y88.540 E0.02070
G1 X67.084 Y88.044 E0.03923
```

### Temperature check
An `M105` is send. The response over the serial port is read as usual, checking for a temperature message. We support formats like:
```
ok T:19.1 /0.0 B:0.0 /0.0 @:0 B@:0
T:19.51 B:-1.00 @:0
T:19.5 E:0 W:?
```
