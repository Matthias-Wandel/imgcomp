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

    delay = 2 # miliseconds
    GPIO.output(g_enable, 0) # enable

    if steps < 0:
        steps = -steps
        GPIO.output(g_dir, 0)
    else:
        GPIO.output(g_dir, 1)

    time.sleep(0.01)

    for x in range (0, steps):
        GPIO.output(g_clk,1)
        duse = delay/2000;
        fromend = min(x*.3, steps-x)
        if fromend < 30: duse = duse * 1.3
        if fromend < 15: duse = duse * 1.3
        GPIO.output(g_clk,0)
        time.sleep(duse)

def move_to_deg(deg):
    steps_per_deg = 800.0/365
    global steppos;
    newstep = int(deg * steps_per_deg)
    move_stepper(newstep-steppos)
    steppos = newstep

def cleanAndExit():
    GPIO.output(g_enable, 1) # turn off stepper
    sys.exit()

steppos = 0

#===========================================================================================
# Find aspistill process
#===========================================================================================
def get_raspistill_pid():
    proc = subprocess.Popen(["pidof","raspistill"], stdout=subprocess.PIPE)
    lines = proc.stdout.readlines()
    if len(lines) != 1:
        print("Could not find raspistill process");
        sys.exit(-1)
    pid = int(lines[0])
    return pid


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
    
    print("UDP from:",addr[0], "x,y=",x,y, "T=",time.perf_counter())
    return x,y

init_stepper()

# Reset motor position by bainging against the stop.
move_to_deg(-180)
move_to_deg(-94)
steppos = 0

raspistill_pid = get_raspistill_pid()

Open_Socket()

while 1:    
    ready = select.select([rxSocket], [], [], 2)

    if ready[0]:
        Process_UDP()
    else:
        print("nothing.  t=",time.perf_counter())
        os.kill(raspistill_pid, signal.SIGUSR1) # Tell raspistill to take shapshot
        

# Notes:
# * Using signals to tell raspistill when to take a picture so that we don't pan while taking a picture.
#
# * Should still signal to imgcomp that the camera as ben panned (ignore an image, reset fatigue map)
#   If I do this, that makes avoiding photos during panning redundant cause I can just throw one out.
#
# * if I start raspistill from this script, then becomes a problem if raspistill is restarted, or not?
#     name raspistill is hard coded in C program in a few places.
# 
#  So...  Start pan.py script from crontab.  Don't try to trigger or raspistill from pan.py
#         But DO signal to imgcomp when the camera has been panned.  
#         But how to signal that?  Touch a file in /ramdisk, cause
#         imgcomp reads the whole directory every second already (in DoDirectoryFunc)

