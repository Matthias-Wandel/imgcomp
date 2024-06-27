#!/usr/bin/python3
# Script for panning using camera controlled by two little servos
#
# This script receives UDP indicating where motion was seen, analyze where the
# action is and pan the camera towards it using the servo motor gimbal.  But will
# only turn camera after a while to not jitter around too much.
#
# It's in python because I will probably tweak the parameters a lot and that makes
# it easier.  I don't expect anyone other than me to ever use this script.
#
# Matthias Wandel Jun 2024

import socket, select, os, sys, signal, subprocess, time

import RPi.GPIO as GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
g_pan = 24
g_tilt = 23

#===========================================================================================
# Servo routines
#===========================================================================================
def init_servo():
    # Motor IO lines
    GPIO.setup(g_pan, GPIO.OUT, initial=False) # pan
    GPIO.setup(g_tilt, GPIO.OUT)               # up/down tilt

def move_to_deg(pan, tilt):
    # Pan is degrees, -135 to 135 degrees, positive is clockwise
    # Tilt is in -60 to 60 degrees, positive is up
    print("set_position",pan,tilt)

    for x in range (0,90): # need a few pulses to get going.
        # Do pan
        GPIO.output(g_pan, 1)
        time.sleep(-pan/135000+0.0015) # Pulse 1 to 2 ms in length
        GPIO.output(g_pan, 0)

        # Do tilt
        GPIO.output(g_tilt, 1)
        time.sleep(tilt/100000+0.00105) # Pulse 1 to 2 ms in length
        GPIO.output(g_tilt, 0)
        time.sleep(0.001)


#===========================================================================================
# Stuff for receiging motion indication via UDP
#===========================================================================================
def Open_Socket():
    global rxSocket
    print("Open socket")
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
        return 0,0,-1


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

    if other_cam == 0 and time.time()-LastPanTime < 2:
        print("Less than 2 sec since pan, ignore own UDP");
        return 0,0,-1

    return x,y, other_cam

init_servo()

MotionBinsH = [0]*7
MotionBinsV = [0]*5

BinDegsH = [-90,-60,-30,0,30,60,90]
BinDegsV = [-35,-20,-5,10,25]
HomeBinHNo = 3
HomeBinVNo = 2
BinAimedHWas = 0
BinAimedVWas = 0
BinAimedH = HomeBinHNo
BinAimedV = HomeBinVNo

Open_Socket()

IsIdle = False

while 1:
    ready = select.select([rxSocket], [], [], 2)

    if ready[0]:
        x,y, other = Process_UDP()
        IsIdle = False

        if x < -250:
            BinAddH = BinAimedH - 1
        elif x > 250:
            BinAddH = BinAimedH + 1
        else:
            BinAddH = BinAimedH

        if BinAddH >= 0 and BinAddH < len(MotionBinsH):
            MotionBinsH[BinAddH] += 100

        if y < -250:
            BinAddV = BinAimedV - 1
        elif y > 250:
            BinAddV = BinAimedV + 1
        else:
            BinAddV = BinAimedV

        if BinAddV >= 0 and BinAddV < len(MotionBinsV):
            MotionBinsV[BinAddV] += 100



    for x in range(0, len(MotionBinsH)):
        # decay the motion bins.
        MotionBinsH[x] = int(MotionBinsH[x] * 0.8) # Store integer, easier to read

    for x in range(0, len(MotionBinsV)):
        # decay the motion bins.
        MotionBinsV[x] = int(MotionBinsV[x] * 0.8)

    print("H:",end="")
    for num in MotionBinsH:
        print(" %3d"%(num), end="")
    print("    V:",end="")
    for num in MotionBinsV:
        print(" %3d"%(num), end="")
    print ("")

    RePan = False
    maxH = 0
    maxpH = -1
    for x in range(0, len(MotionBinsH)):
        if MotionBinsH[x]  > maxH:
            maxH = MotionBinsH[x]
            maxpH = x
    maxV = 0
    maxpV = -1
    for x in range(0, len(MotionBinsV)):
        if MotionBinsV[x]  > maxV:
            maxV = MotionBinsV[x]
            maxpV = x

    if maxH < 20:
        # No significant motion for a while.  Return to center.
        if not IsIdle:
            print("No action for a while, back home position")
            BinAimedH = HomeBinHNo
            BinAimedV = HomeBinVNo
            RePan = True
            IsIdle = True
    else:
        if maxH > 200 and maxpH != BinAimedH and maxH > MotionBinsH[BinAimedH]*1.2:
            print("Horizontal pan needed")
            RePan = True

        if maxV > 200 and maxpV != BinAimedV and maxV > MotionBinsV[BinAimedV]*1.2:
            print("Vertical pan needed")
            RePan = True

        if RePan:
            BinAimedH = maxpH
            BinAimedV = maxpV

    if RePan:
        if BinAimedH == BinAimedHWas and BinAimedVWas == BinAimedV:
            print("Already aimed at (%d,%d)"%(BinAimedHWas,BinAimedVWas))
        else:
            print("Move view (%d,%d)"%(BinAimedHWas,BinAimedVWas),"to (%d,%d)"%(BinAimedH,BinAimedV))

            open("/ramdisk/angle", 'a').close() # Tell imgcomp that angle was adjusted
            move_to_deg(BinDegsH[BinAimedH],BinDegsV[BinAimedV])
            BinAimedVWas = BinAimedV
            BinAimedHWas = BinAimedH
            with open("/ramdisk/angle", 'a') as f:
                # In case panning took a long time, tell imgcomp again.
                print("Pan=%d tilt=%d"%(BinDegsH[BinAimedH],BinDegsV[BinAimedV]),file=f)
                f.close()



