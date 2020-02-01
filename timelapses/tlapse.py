#!/usr/bin/python3
# Script to make timelapses from output from my imgcomp program
# Uses subtitles to add timestamps in the bottom right corner.
import  sys, glob, os, math

framerate = 7.5
timeunit = 1/framerate
bitrate = framerate*160 #in kilobits


filepath = "*.jpg"
if len(sys.argv) > 1: filepath = sys.argv[1]

# Come up with a reasonable output name.
outname = os.path.dirname(filepath)
if outname == "": outname = "out"
if outname[1] == ":": outname = outname[2:]
outname = outname.replace("\\","_").replace("/","_")
if outname[0] == "_": outname = outname[1:]
if outname == "": outname = "out"
outname = outname + ".mp4"

print("Frame rate:",framerate);

# Write header for the timestamps subtitles file.
tssubs = open("tssubs.ass","w")
tssubs.write("""[Script Info]
ScriptType: v4.00+\nWrapStyle: 0\nScaledBorderAndShadow: no\nYCbCr Matrix: None
\n[V4+ Styles]
Format:Name,Fontname ,Fontsize,Bold,PrimaryColour,OutlineColour,ScaleX,ScaleY,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding
Style: Def ,monospace,16      ,1   ,&H80FFFF     ,&H000000     ,100   ,100   ,2          ,6      ,0     ,3        ,1      ,1      ,1      ,1
\n[Events]
Format:   Start,   End,     Style,Text\n""")
#Dialogue: 0:00:1.9,0:00:2.2,Def,Hello
#Dialogue: 0:00:2.2,0:00:3.0,Def,World
#Dialogue: 0:00:3.9,0:00:4.1,Def,Again!

vidtime = 0.0
lastsubtotime = 0;

def WriteEntry(filename, duration):
    global vidtime, lastsubtotime
    imglist.write("file '"+filename+"'\n")
    imglist.write("duration "+str(duration)+"\n");
    
    label = bi[5:7]+":"+bi[7:9]+":"+bi[9:11]
   
    if vidtime >= lastsubtotime:
        fromtime = lastsubtotime
        totime = vidtime+duration
        if totime-fromtime < 1: totime = fromtime+1 # Must show at least one second
    
        fromstr = "%d:%02d:%04.1f"%(int(fromtime/3600),int((fromtime/60)%60),math.modf(fromtime/60)[0]*60)
        tostr   = "%d:%02d:%04.1f"%(int(totime/3600),int((totime/60)%60),math.modf(totime/60)[0]*60)
        dialogue = "Dialogue: "+fromstr+","+tostr+",Def,"+label
        #print (dialogue)
        tssubs.write(dialogue+"\n")
        lastsubtotime = totime
    vidtime += duration

images = glob.glob(filepath)
print ("Number of images:",len(images))
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
    
    if prevname != "":
        gap = second-prevsecond
    
        duration = timeunit
        if gap > 5: duration = timeunit*2
    
        WriteEntry(prevname,duration)
    
        if gap > 120:
            # Assume I left the shop, show black.
            WriteEntry("black.jpg", 0.3)
    else:
        prevsecond = second
   
    
    prevsecond = second
    prevname = i
    
    a += 1

if prevname != "": WriteEntry(prevname, timeunit*2)

print ("Video output seconds:",vidtime)
print ("Output name:",outname)
print ("----------------------------------------");
    
imglist.close()
tssubs.close()

cmd = "ffmpeg -f concat -safe 0 -i imglist.txt -vf subtitles=tssubs.ass -r "+str(framerate)+" -b:v "+str(bitrate)+"K "+outname
os.system(cmd)


