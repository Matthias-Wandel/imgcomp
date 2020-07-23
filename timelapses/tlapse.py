#!/usr/bin/python3
# Script to make timelapses from output from my imgcomp program using ffmpeg.
# Makes use of subtitling feature to add timestamps in the bottom right corner.
import  sys, glob, os, math, argparse, shutil

if shutil.which("ffmpeg") == None:
    print("[Error]: ffmpeg not found in PATH", file=sys.stderr)
    exit(1)

parser = argparse.ArgumentParser(
    description='Make timelapses with images from imgcomp using ffmpeg.',
    allow_abbrev=True,
)
parser.add_argument(
    '-o',
    '--outputfile',
    required=False,
    help='Output file name.',
)
parser.add_argument(
    '-p',
    '--pathname',
    type=str,
    required=False,
    default='',
    help='Path to a directory containing imgcomp images. Default is current dir.',
)
parser.add_argument(
    '-k',
    '--keep-assets',
    required=False,
    default=False,
    action='store_true',
    help='Flag to keep both the pic list and timestamps header files.',
)
parser.add_argument(
    '-n',
    '--no-timestamp',
    required=False,
    default=False,
    action='store_true',
    help='Flag to disable timestamps in timelapse output video.',
)

args = parser.parse_args()
#print('[DEBUG]:', args)

framerate = 7.5         # output frame rate.
timeunit = 1/framerate
bitrate = framerate*160 # Vary bitrate with frame rate.

outname = os.path.dirname(args.pathname)
if args.outputfile:
    outname = args.outputfile
else:
    # Come up with a reasonable output name.
    if outname == "": outname = "out"
    if outname[1] == ":": outname = outname[2:]
    outname = outname.replace("\\","_").replace("/","_")
    if outname[0] == "_": outname = outname[1:]
    if outname == "": outname = "out"
    outname = outname + ".mp4"

print("Frame rate:",framerate);

tssubs = None
if not args.no_timestamp:
    # Write header for the timestamps subtitles file.
    tssubs = open("tssubs.ass","w")
    tssubs.write("""[Script Info]
ScriptType: v4.00+\nWrapStyle: 0\nScaledBorderAndShadow: no\nYCbCr Matrix: None
\n[V4+ Styles]
Format:Name,Fontname ,Fontsize,Bold,PrimaryColour,OutlineColour,ScaleX,ScaleY,BorderStyle,Outline,Shadow,Alignment,MarginL,MarginR,MarginV,Encoding
Style: Def ,monospace,16      ,1   ,&H80FFFF     ,&H000000     ,100   ,100   ,2          ,6      ,0     ,3        ,1      ,1      ,1      ,1
\n[Events]
Format:   Start,   End,     Style,Text\n""")

    # Entries will look like this:
    #Dialogue: 0:00:1.9,0:00:2.2,Def,Hello
    #Dialogue: 0:00:2.2,0:00:3.0,Def,World
    #Dialogue: 0:00:3.9,0:00:4.1,Def,Again!

vidtime = 0.0
lastsubtotime = 0;

def WriteEntry(filename, duration):
    global vidtime, lastsubtotime
    imglist.write("file '"+filename+"'\n")
    imglist.write("duration "+str(duration)+"\n");

    if not args.no_timestamp:
        label = bi[5:7]+":"+bi[7:9]+":"+bi[9:11]
        if vidtime >= lastsubtotime:
            fromtime = lastsubtotime
            totime = vidtime+duration
            if totime-fromtime < 1: totime = fromtime+1 # Must show at least one second or it won't show up.

            fromstr = "%d:%02d:%04.1f"%(int(fromtime/3600),int((fromtime/60)%60),math.modf(fromtime/60)[0]*60)
            tostr   = "%d:%02d:%04.1f"%(int(totime/3600),int((totime/60)%60),math.modf(totime/60)[0]*60)
            dialogue = "Dialogue: "+fromstr+","+tostr+",Def,"+label
            #print (dialogue)
            tssubs.write(dialogue+"\n")
            lastsubtotime = totime
        vidtime += duration

pathname = args.pathname
if pathname and pathname[len(pathname)-1] != '/':
    pathname += '/*.jpg'
else:
    pathname += "*.jpg"

images = glob.glob(pathname)
print ("Number of images:",len(images))
images = sorted(images)
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

        if gap > 120 and os.path.isfile("black.jpg"):
            # Assume I left the shop, show black.
            WriteEntry("black.jpg", 0.3)

    prevsecond = second
    prevname = i


if prevname != "": WriteEntry(prevname, timeunit*2)

print ("Video output seconds:",vidtime)
print ("Output name:",outname)
print ("----------------------------------------");

imglist.close()
if tssubs is not None:
    tssubs.close()

subtitles = '' if args.no_timestamp else '-vf subtitles=tssubs.ass'
cmd = "ffmpeg -f concat -safe 0 -i imglist.txt "+subtitles+" -r "+str(framerate)+" -b:v "+str(bitrate)+"K "+outname
#print('[DEBUG]:', cmd)
os.system(cmd)

if not args.keep_assets:
    os.remove('imglist.txt')
    if not args.no_timestamp:
        os.remove('tssubs.ass')
