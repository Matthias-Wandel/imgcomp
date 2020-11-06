#!/usr/bin/python3
# Script to make timelapses from directories of images from imgcomp.
# Shows time below image, plus animated "actagram" view.
#
# Matthias Wandel Nov 2020
from PIL import Image, ImageDraw, ImageFont
from subprocess import Popen, PIPE
import sys, glob, os, re, argparse, shutil
framerate = 7.5

if shutil.which("ffmpeg") == None:
    print("[Error]: ffmpeg not found in PATH", file=sys.stderr)
    exit(1)

parser = argparse.ArgumentParser(
    description='Make timelapses with images from imgcomp using ffmpeg.',
    allow_abbrev=True,
)
parser.add_argument(
    '-o','--outfile',
    required=False,
    help='Output file name.',
)

parser.add_argument(
    '-nd', '--no-dupframes',
    required=False, default=False,action='store_true',
    help='Disable duplicating frame after time gap',
)

parser.add_argument(
    '-n', '--no-timestamp',
    required=False, default=False,action='store_true',
    help='Flag to disable timestamps and actagram in timelapse output video.',
)

parser.add_argument('dirs', metavar='dirs', type=str, nargs='+',
                    help='Directories to read')

args = parser.parse_args()
#print (args)

no_dupframes = args.no_dupframes

if args.outfile:
    outname = args.outfile
else:
    # Come up with a reasonable output name.
    outname = "timelapse"
    if len(args.dirs) == 1: outname = args.dirs[0]
    
    if outname[1] == ":": outname = outname[2:]
    if outname == "": outname = "timelapse"

    outname = outname.replace("\\","_").replace("/","_")
    if outname[0] == "_": outname = outname[1:]
    if outname == "": outname = "timelapse"
    outname = outname + ".avi"

print ("out name:" ,outname)
print (args.dirs)


images = []
for pn in args.dirs:
    if pn.find("*") == -1 and pn.find("?") == -1:
        if pn[-1] != '/' and pn[-1] != ":":
            pn += '/*.jpg'
        else:
            pn += "*.jpg"

    dir_images = glob.glob(pn)
    print("Dir:",pn,"has",len(dir_images),"images")
    dir_images = sorted(dir_images)
    images = images + dir_images

frames = 0

timestamps = []   # one entry per output frame
frameseconds = [] # one entry per output frame

seconds = [] # one entry per image

# Make image list to specify input frames for the timelapse part.
with open("imglist.txt","w") as imglist:
    prevname = ""
    imglist2 = []

    def WriteEntry(filename, imgframes, second):
        global frames, timestamps
        for i in range (imgframes):
            imglist.write("file '"+filename+"'\n")
            timestamps.append(os.path.basename(filename)[0:11])
            frameseconds.append(second)
            frames += 1

    nr = re.compile(r"""(\d\d\d\d)-(\d\d)(\d\d)(\d\d)""", re.ASCII)
    prevsecond = -100

    for i in images:
        bi= os.path.basename(i)

        m = nr.match(bi)
        if not m: 
            print("Skipping file:",i)
            continue

        second = int(m.group(2))*3600+int(m.group(3))*60+int(m.group(4))

        nf = 1 if second-prevsecond <= 10 or no_dupframes else 2
        prevsecond = second
            
        seconds.append(second)
        imglist2.append(i)
        WriteEntry(i,nf, second)

    images = imglist2 # Replace list with one with unused files removed.

print ("Number of images:",len(images))
size = Image.open(images[0]).size
size2 = Image.open(images[-1]).size

if size != size2:
    print("Size changes from %dx%d to %dx%d" % (size+size2))
    if size2[0] > size[0]: size=size2

imgwidth,imgheight = size

print("Image size: ",size)
print("Output frames = ",frames)

firstsec = int(seconds[0] /3600)*3600
hours = int((seconds[-1]+3600-1-firstsec)/3600)
print (hours)

# Parameters for histogram bagraph.
HIST_BINS = 240*hours
SEC_PER_BIN = 3600/240
ActBins = [0] * HIST_BINS
hist_left = 300
hist_width = imgwidth-hist_left
per_hist_bar = hist_width/HIST_BINS
actagram_top=5
actagram_height=60
actagram_bot = actagram_top+actagram_height

max_bin = 6;# maximun used to scale histogram within canvas height, if < 6 use 6.
for sec in seconds:
    bin = int((sec-firstsec)/SEC_PER_BIN)
    if bin < HIST_BINS:
        n = ActBins[bin] + 1
        ActBins[bin] = n
    if max_bin < n: max_bin = n;

def MakeActagram():
    Actagram = Image.new("RGB", (imgwidth, actagram_top+actagram_height), (0,0,0))
    imgdraw = ImageDraw.Draw(Actagram)

    # draw 10 minute scale stripe background as background layer.
    for a in range (0,HIST_BINS,int(1200/SEC_PER_BIN)):
        histX = a*per_hist_bar+hist_left
        fc = "#303030"
        imgdraw.rectangle([(histX,actagram_top),
                           (histX+per_hist_bar*600/SEC_PER_BIN,actagram_bot)],
                           fill=fc)
    # Draw the bars.
    for a in range(HIST_BINS):
        b = ActBins[a];
        if b == 0: continue

        rectHeight = b/max_bin*actagram_height;
        
        histX = a*per_hist_bar+hist_left
        imgdraw.rectangle([(histX,actagram_bot),(histX + per_hist_bar-2,actagram_bot-rectHeight)], fill="#a0a0a0")

    #Actagram.show()
    return Actagram

pargs = ['ffmpeg', '-y', '-hide_banner']

# Parameters for timelapse images
pargs = pargs + ['-f', 'concat', '-safe', '0', '-r', str(framerate), '-i', 'imglist.txt']

if not args.no_timestamp:
    pargs = pargs + [
     # Parameters for actagram
     '-f', 'image2pipe', '-vcodec', 'mjpeg', '-r', str(framerate), '-i', '-',

     # How to combine them.  Apply scaling in case I reconfigured image size during the hour.
     '-filter_complex', '[0:v]scale=%d:%d[t]'%size + ';[t][1]vstack'  ]

pargs = pargs + [
     # Output prameters
     '-vcodec', 'mpeg4', '-qscale', '7', '-r', str(framerate), outname]

p = Popen(pargs, stdin=PIPE)

if not args.no_timestamp:
    Actagram = MakeActagram()

    textsize = 40
    if sys.platform == "linux":
        font = ImageFont.truetype("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", textsize)
    else:
        font = ImageFont.truetype(r'C:\Users\System-Pc\Desktop\arial.ttf', textsize)

    for i in range(frames):
        im = Actagram.copy()
        draw = ImageDraw.Draw(im)
        draw.text((0,(actagram_height-textsize)/2+actagram_top), timestamps[i], font = font, align ="left")

        second = frameseconds[i]
        imx = (second-firstsec)/SEC_PER_BIN*per_hist_bar+hist_left
        draw.rectangle([(imx-1,actagram_top),(imx+1,actagram_bot)], fill="#ffff00")

        im.save(p.stdin, 'JPEG', quality=90)
        
p.stdin.close()
p.wait()

#Still to do:
# Specify output frame rate
# Specify image quality
