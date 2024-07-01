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
g_pan = 10
g_tilt = 9

current = [0,0]
#===========================================================================================
# Servo routines
#===========================================================================================
def init_servo():
    # Motor IO lines
    print("init servo")
    GPIO.setup(g_pan, GPIO.OUT, initial=False) # pan
    GPIO.setup(g_tilt, GPIO.OUT)               # up/down tilt

def move_to_deg(pan, tilt):
    # Slowly ramp servos so they don't run as fast and make less noise.

    # Pan is degrees, -135 to 135 degrees, positive is clockwise
    # Tilt is in -60 to 60 degrees, positive is up
    print("set_position",pan,tilt)

    gp = [g_pan, g_tilt]
    stepsize = 0.000005 # seconds
    dwell = [5,5]
    target = [pan/135000+0.0015, -tilt/100000+0.00105]
    global current
    if current[0] == 0:
        # First invocation, don't know current angle, so just dwell a while.
        current = target[0:2]
        dwell = [100,100]
    states = [0,0]

    #print(current, target)

    while True:
        totpulse = 0
        for ch in range(0,2):
            state = states[ch]
            if state == 0 or state == 2: # Initial and final dwell
                dwell[ch] -= 1
                if dwell[ch] <= 0:
                    state += 1
                    #print("        "*ch*3+"dwell end")
                pulselen = current[ch]
            elif state == 1: # Ramp state
                diff = target[ch]-current[ch]
                if abs(diff) < stepsize: # finished ramping
                    state = 2
                    dwell[ch] = 2
                    current[ch] = target[ch]
                else:
                    if diff > 0:
                        current[ch] += stepsize
                    else:
                        current[ch] -= stepsize
                pulselen = current[ch]
            elif state == 3: # dwell-ramp-dwell completed.
                pulselen = 0
                continue

            #if ch: print("                        ",end="")
            #print("c:%d s:%d p: %5.4f"%(ch,state,pulselen*1000))

            #print("\t\t\t"*ch,"pulse %d %6.4f ms"%(ch, pulselen))
            # Send the pulse to this motor
            GPIO.output(gp[ch], 1)
            time.sleep(pulselen)
            GPIO.output(gp[ch], 0)

            totpulse += pulselen
            states[ch] = state

        if totpulse:
            # Sleep balance of time.
            time.sleep(0.005-totpulse)
        else:
            break # done

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


BinDegsH = [-115,-90,-65,-40,-15,10,35,60,85,110,135] # Pos 3 is workbench, 9 is default.
BinDegsV = [-57,-41,-29]
HomeBinHNo = 9
HomeBinVNo = 2
WorkbenchBinHNo = 3

if len(sys.argv) > 1:
    # manual aiming, for testing.
    pan = BinDegsH[int(sys.argv[1])]
    tilt = -20
    if len(sys.argv) == 3:
        tilt = BinDegsV[int(sys.argv[2])]

    print("Manual aim to pan=%d tilt=%d"%(pan,tilt))
    move_to_deg(pan,tilt)
    sys.exit(0)


MotionBinsH = [0]*len(BinDegsH)
MotionBinsV = [0]*len(BinDegsV)
BinAimedHWas = 0
BinAimedVWas = 0
BinAimedH = 0
BinAimedV = 0

Open_Socket()

IsIdle = False
LastPanTime = time.time()

while 1:
    for x in range(0, len(MotionBinsH)):
        # decay the motion bins.
        MotionBinsH[x] = int(MotionBinsH[x] * 0.8) # Store integer, easier to read

    for x in range(0, len(MotionBinsV)):
        # decay the motion bins.
        MotionBinsV[x] = int(MotionBinsV[x] * 0.8)


    ready = select.select([rxSocket], [], [], 4)

    if ready[0]:
        x,y, other = Process_UDP()
        if other == 1:
            # My other camera saw motion near workbench
            print("other")
            if BinAimedH > WorkbenchBinHNo+2 and MotionBinsH[WorkbenchBinHNo] < 100:
                MotionBinsH[WorkbenchBinHNo] += 120
                if MotionBinsV[2] < 20: MotionBinsV[2] = 20
        else:

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

        IsIdle = False


    print("H:",end="")
    for num in MotionBinsH:
        print(" %3d"%(num), end="")
    print("    V:",end="")
    for num in MotionBinsV:
        print(" %3d"%(num), end="")
    print ("")

    RePan = False
    maxH = 0
    maxpH = HomeBinHNo
    for x in range(0, len(MotionBinsH)):
        if MotionBinsH[x]  > maxH:
            maxH = MotionBinsH[x]
            maxpH = x
    maxV = 0
    maxpV = HomeBinVNo
    for x in range(0, len(MotionBinsV)):
        if MotionBinsV[x]  > maxV:
            maxV = MotionBinsV[x]
            maxpV = x

    if maxH < 20:
        # No significant motion for a while.  Return to home position.
        if not IsIdle:
            print("No action, home pos")
            BinAimedH = HomeBinHNo
            BinAimedV = HomeBinVNo
            RePan = True
            IsIdle = True
    else:
        if maxH >= 200 and maxpH != BinAimedH and maxH > MotionBinsH[BinAimedH]*1.2:
            print("Horizontal pan needed")
            RePan = True

        if maxV >= 200 and maxpV != BinAimedV and maxV > MotionBinsV[BinAimedV]*1.2:
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
            LastPanTime = time.time()
            BinAimedVWas = BinAimedV
            BinAimedHWas = BinAimedH
            with open("/ramdisk/angle", 'a') as f:
                # In case panning took a long time, tell imgcomp again.
                print("Pan=%d tilt=%d"%(BinDegsH[BinAimedH],BinDegsV[BinAimedV]),file=f)
                f.close()



