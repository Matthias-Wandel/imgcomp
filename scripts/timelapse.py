#!/usr/bin/python3
# Script to make timelapses from directories of images from imgcomp.
# Shows time below image, plus animated "actagram" view.
#
# Matthias Wandel Nov 2020
from PIL import Image, ImageDraw, ImageFont
from subprocess import Popen, PIPE
import sys, glob, os
framerate = 10
duration = 10


images = glob.glob("images/*.jpg")
print ("Number of images:",len(images))
images = sorted(images)
frames = 0

timestamps = []   # in output frames
frameseconds = [] # in output frames

seconds = [] # in input frames

# Make image list to specify input frames for the timelapse part.
with open("imglist.txt","w") as imglist:
    prevname = ""

    def WriteEntry(filename, imgframes, second):
        global vidtime, lastsubtotime, frames, timestamps
        imglist.write("file '"+filename+"'\n")
        imglist.write("duration "+str(imgframes/framerate)+"\n");
        frames += imgframes
        for i in range (imgframes):
            timestamps.append(os.path.basename(filename)[0:11])
            frameseconds.append(second)


    for i in images:
        bi= os.path.basename(i)

        try:
            second = int(bi[5:7])*3600
            second += int(bi[7:9])*60 + int(bi[9:11])
            seconds.append(second)
        except:
            # Not a numbered file.  Skip
            continue

        if prevname != "":
            gap = second-prevsecond

            nf = 1
            if gap > 5: nf = 2

            WriteEntry(prevname,nf, prevsecond)

        prevsecond = second
        prevname = i

    if prevname != "": WriteEntry(prevname, 3, prevsecond)


print("Output frames = ",frames)
#print (timestamps)

firstsec = int(seconds[0] /3600)*3600

print(seconds)
print ("Timestamp array len",len(timestamps))



# Create image of histogram bargraph to go here
HIST_BINS = 240
imgwidth = 2400
ActBins = [0] * HIST_BINS
per_hist_bar = imgwidth/HIST_BINS
BARS_HEIGHT=100

max_bin = 6;# maximun used to scale histogram within canvas height
for sec in seconds:
    binfl = (sec-firstsec)*HIST_BINS/3600
    bin = int(binfl)
    ActBins[bin] += 1
    if max_bin < ActBins[bin]: max_bin = ActBins[bin];

def MakeActagram():
    Actagram = Image.new("RGB", (imgwidth, 100), (30,30,30))

    # create rectangle image
    imgdraw = ImageDraw.Draw(Actagram)
    shape = [(200, 2), (600,20)]
    imgdraw.rectangle(shape, fill ="#ff2233", outline ="red")


    ## draw 10 minute scale stripe background as first layer
    for a in range (0,60,10):
        histX = a*HIST_BINS/60*per_hist_bar
        fc = "#000000"
        if a % 20 == 0: fc = "#282828"
        imgdraw.rectangle([(histX,0),(histX+per_hist_bar*HIST_BINS/6,100)], fill=fc)

    for a in range(HIST_BINS):
        b = ActBins[a];
        if b == 0: continue

        rectHeight = b/max_bin*BARS_HEIGHT;
        print(a)

        imgdraw.rectangle([(a*per_hist_bar,100),((a+1)*per_hist_bar+1,100-rectHeight)], fill="#a0a0a0")

    #Actagram.show()
    return Actagram

Actagram = MakeActagram()

p = Popen(['ffmpeg', '-y',
     # Parameters for timelapse images
     '-f', 'concat', '-safe', '0', '-i', 'imglist.txt',

     # Parameters for actagram
     '-f', 'image2pipe', '-vcodec', 'mjpeg', '-r', str(framerate), '-i', '-',

     # How to combine them
     '-filter_complex', 'vstack',

     # Output prameters
     '-vcodec', 'mpeg4', '-qscale', '3', '-r', str(framerate), 'video.avi'], stdin=PIPE)

font = ImageFont.truetype(r'C:\Users\System-Pc\Desktop\arial.ttf', 40)
im = Image.new("RGB", (2400, 50), (30,30,30))
for i in range(frames):
    im = Actagram.copy()
    draw = ImageDraw.Draw(im)
    draw.text((5, 5), timestamps[i], font = font, align ="left")

    second = frameseconds[i]
    imx = (second-firstsec)/3600*imgwidth
    draw.rectangle([(imx-1,0),(imx+1,100)], fill="#ff00ff")

    im.save(p.stdin, 'JPEG', quality=90)
p.stdin.close()
p.wait()

#Still to do:
# Get size from images and scale actagram accordingly
# Make actagram not full width to leave room for timestamp
# Test if images are not all same size
# Handle doing more than one hour's worth, from multiple directories
# --outputfile option
# --pathname option
# --no-timestamp option
# --no-actagram option