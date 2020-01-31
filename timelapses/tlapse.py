#!/usr/bin/python3
# Script to make timelapses with timestamps in them.
import  sys, glob, os

filepath = "*.jpg"

# Write header for the timestamps subtitles file.
#tssubs = open("tssubs.ass","w")
#tssubs.write("""[Script Info]
#ScriptType: v4.00+\nWrapStyle: 0\nScaledBorderAndShadow: no\nYCbCr Matrix: None
#\n[V4+ Styles]
#Format:Name,Fontname ,Fontsize,Bold,PrimaryColour,OutlineColour,ScaleX,ScaleY,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding
#Style: Def ,monospace,16      ,1   ,&H80FFFF     ,&H000000     ,100   ,100   ,2          ,6      ,0     ,3        ,1      ,1      ,1      ,1
#\n[Events]
#Format:   Start,   End,     Style,Text\n""")

#Entries:
#Dialogue: 0:00:1.9,0:00:2.2,Def,Hello
#Dialogue: 0:00:2.2,0:00:3.0,Def,World
#Dialogue: 0:00:3.9,0:00:4.1,Def,Again!



vidtime = 0.0


def WriteEntry(filename, duration):
    global vidtime
    imglist.write("file '"+filename+"'\n")
    imglist.write("duration "+str(duration)+"\n");
    vidtime += duration

images = glob.glob(filepath)
print (images)
images = sorted(images)
a = 0

imglist = open("imglist.txt","w")

prevsecond = 0
prevname = ""
for i in images:
    bi= os.path.basename(i)
    
    try:
        second = int(bi[5:7])*3600
        second = int(bi[7:9])*60 + int(bi[9:11])
    except:
        # Not a numbered file.  Skip
        continue
        
    #print (second)
    #label = bi[5:7]+":"+bi[7:9]+":"+bi[9:11]
    
    if prevname != "":
        gap = second-prevsecond
    
        duration = 0.1
        if gap > 5: duration = 0.2
    
        WriteEntry(prevname,duration)
    
        if gap > 120:
            # Assume I left the shop, show black.
            WriteEntry("blak.jpg", 0.2)
   
    
    prevsecond = second
    prevname = i
    
    a += 1
    if a > 300: break

if prevname != "": WriteEntry(prevname, 0.1)

print (vidtime)
    
imglist.close()

cmd = "ffmpeg -f concat -safe 0 -i imglist.txt -r 5 -b:v 1000K out.mp4"

os.system(cmd)


