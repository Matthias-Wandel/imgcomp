#!/usr/bin/env python
# Python script for testing three stepper drivers hooked up to raspberry pi

# Motor 1:
# Enable: GPIO 02
# Dir:    GPIO 03
# Clock:  GPIO 04

# Motor 2:
# Enable: GPIO 15
# Dir:    GPIO 17
# Clock:  GPIO 18

# Motor 3:
# Enable: GPIO 22
# Dir:    GPIO 23
# Clock:  GPIO 24


import RPi.GPIO as GPIO, sys
from time import sleep
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# motor 1
GPIO.setup(2, GPIO.OUT, initial=True)
GPIO.setup(3, GPIO.OUT)
GPIO.setup(4, GPIO.OUT, initial=False)

# motor 2
GPIO.setup(15, GPIO.OUT, initial=True)
GPIO.setup(17, GPIO.OUT)
GPIO.setup(18, GPIO.OUT, initial=False)

# motor 3
GPIO.setup(22, GPIO.OUT, initial=True)
GPIO.setup(23, GPIO.OUT)
GPIO.setup(24, GPIO.OUT, initial=False)


duration = 0.5
steps = 480
motor = 1
dir = 1
delay = 0.5 # Milliseconds

if len(sys.argv) > 1:
    motor = int(float(sys.argv[1]))
    print("Motor: ",motor)
    
if len(sys.argv) > 2 and sys.argv[2] == "l":
    print("Dir: left")
    dir = 0
elif len(sys.argv) > 2 and sys.argv[2] == "r":
    print("Dir: right")
    dir = 1
    
if len(sys.argv) > 3:
    steps = int(float(sys.argv[3]))

if len (sys.argv) > 4:
    delay = float(sys.argv[4]) 
print (delay)
delay = delay / 1000;
    
if motor == 1:
    g_enable = 2
    g_dir = 3
    g_clk = 4
if motor == 2:
    g_enable = 15
    g_dir = 17
    g_clk = 18
if motor == 3:
    g_enable = 22
    g_dir = 23
    g_clk = 24

print("Motor:"+str(motor)+ "  Dir:"+str(dir)+"  Steps:"+str(steps))
GPIO.output(g_enable, 0) # enable
shots = 2
for s in range (0,shots):
    GPIO.output(g_dir, dir)
    duse = delay*3
    for x in range (0, steps):
        GPIO.output(g_clk,1)
        sleep(0.0005)
        if duse == 0: raw_input(); print x
        GPIO.output(g_clk,0)
        sleep(duse)
        if x < 5:
            duse = delay*2
        else:
            duse = delay
        duse = delay;
    duse = delay*2
    GPIO.output(g_dir, 0)
    sleep(0.1)
    for x in range (0, steps):
        GPIO.output(g_clk,1)
        sleep(0.0005)
        if duse == 0: raw_input(); print x
        GPIO.output(g_clk,0)
        sleep(duse)
        duse = delay;
    sleep(0.05)

