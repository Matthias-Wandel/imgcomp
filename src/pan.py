#!/usr/bin/python3
# For my experiments with panning the camera on a stepper motor.
#
# This script receives UDP indicating where motion was seen, analyze where the
# action is and pan the camera towards it by driving a stepper motor
# It's in python because I will probably tweak the parameters a lot and that makes it easier.
# I don't think anyone other than me will ever use this script.
#
# Matthias Wandel August 2022

import socket, select, os, sys, signal, subprocess, time

import RPi.GPIO as GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
g_enable = 26; g_dir = 19; g_clk = 13


#===========================================================================================
# Stepper motor routines
#===========================================================================================
def init_stepper():
    # Motor IO lines
    GPIO.setup(26, GPIO.OUT, initial=True)  # enable
    GPIO.setup(19, GPIO.OUT)                # direction
    GPIO.setup(13, GPIO.OUT, initial=False) # step

    # For reading limit swithc.
    GPIO.setup(6, GPIO.IN,  pull_up_down=GPIO.PUD_UP)

def move_stepper(steps):
    global g_enable, g_dir, g_clk

    delay = 0.6 # miliseconds
    GPIO.output(g_enable, 0) # enable stepper driver
    time.sleep(0.01)
    
    if steps < 0:
        steps = -steps
        GPIO.output(g_dir, 1)
    else:
        GPIO.output(g_dir, 0)

    time.sleep(0.01)

    for x in range (0, steps):
        GPIO.output(g_clk,1)
        duse = delay/2000;
        fromend = min(x*.3, steps-x)
        if fromend < 50: duse = duse * 1.3
        if fromend < 25: duse = duse * 1.3
        GPIO.output(g_clk,0)
        time.sleep(duse)

       
    time.sleep(0.01)
    GPIO.output(g_enable, 1) # turn stepper driver off again.


#===========================================================================================
# Re-aim the camera
#===========================================================================================
def move_to_deg(deg, Bin=-1):
    steps_per_deg = 400.0*8/365
    global steppos, MotionBins
    newstep = int(deg * steps_per_deg)
    
    if newstep == steppos: return
    
    open("/ramdisk/angle", 'a').close() # Tell imgcomp that angle was adjusted
    move_stepper(newstep-steppos)
    steppos = newstep
    with open("/ramdisk/panned", 'a') as f:
        print("Pan[%d]=%d"%(Bin,deg)," Bins=",MotionBins,file=f)
        f.close() # In case panning took a long time, tell imgcomp again.

def cleanAndExit():
    GPIO.output(g_enable, 1) # turn off stepper
    sys.exit()

steppos = 0

#===========================================================================================
# Stuff for receiging motion indication via UDP
#===========================================================================================
def Open_Socket():
    global rxSocket
    portNum = 7777
    rxSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) #UDP

    # Bind to any available address on port *portNum*
    rxSocket.bind(("",portNum))

    # Prevent the socket from blocking until it receives all the data it wants
    # Note: Instead of blocking, it will throw a socket.error exception if it doesn't get any data
    rxSocket.setblocking(0)

    print ("RX socket opened on UDP port ", portNum)

def Process_UDP():
    data,addr = rxSocket.recvfrom(1024)

    # First 2 bytes is ident, check that.
    if data[0] != 0xc1 or data[1] != 0x46:
        print("UDP Wrong ID from", addr)
        return 10000,10000

    # Next 2 bytes is level, ignore.

    # Next 2 bytes is X
    x = data[4] + data[5]*0x100
    if x >= 32768: x = x - 65536 # Sign extend

    # Next 2 bytes is Y
    y = data[6] + data[7]*0x100
    if y >= 32768: y = y - 65536 # sign extend

    other_cam = 0
    print("UDP from:",addr[0], "x,y=",x,y)# "T=",time.perf_counter())
    if addr[0] != "192.168.0.22": other_cam = 1
    return x,y, other_cam

init_stepper()

MotionBins = [0]*10
print("Homing camera angle")
# Reset motor position by bainging against the stop.
move_to_deg(360)
move_to_deg(185)
steppos = 0 # this is now the zero position.

print("Open socket")

Open_Socket()

MainView = 0
BinDegs = [-75,-50,-25,0,25,50,75,105,135,160]
BinAimed = 3

while 1:
    ready = select.select([rxSocket], [], [], 4)

    if ready[0]:
        x,y, other = Process_UDP()

        if other:
            # Perhaps should assume which way the camera is aimed and be more fine grained
            # about angle suggestion
            MainView += 100
        else:
            if x < -250:
                BinAdd = BinAimed - 1
            elif x > 250:
                BinAdd = BinAimed + 1
            else:
                BinAdd = BinAimed

            if BinAdd >= 0 and BinAdd < len(MotionBins):
                MotionBins[BinAdd] += 100

    #else:
        #print("time...")#nothing.  t=",time.perf_counter())

    for x in range(0, len(MotionBins)):
        # decay the motion bins.
        MotionBins[x] = int(MotionBins[x] * 0.8) # Store integer, easier to read
    MainView = int(MainView * 0.85)

    print("   ",MotionBins," M:",MainView)

    max = 0
    maxp = -1
    for x in range(0, len(MotionBins)):
        if MotionBins[x]  > max:
            max = MotionBins[x]
            maxp = x

    if max < 20:
        # No significant motion for a while.  Return to center.
        if (MainView > 250):
            print("Front action, pan to main view")
            BinAimed = 3
        else:
            if BinAimed != 8:
                print("Pan back to rear")
                BinAimed = 8
            
        move_to_deg(BinDegs[BinAimed], BinAimed)
            

    elif maxp != BinAimed and max > 200 and max > MotionBins[BinAimed]*1.2:
        print("Pan to ",maxp)
        BinAimed = maxp
        move_to_deg(BinDegs[BinAimed])


