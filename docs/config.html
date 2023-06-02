<html>
<head>
<title>Imgcomp configuration</title>
<style type=text/css>
  body { font-family: sans-serif; font-size: 100%;}
  div.c { white-space: pre; font-family: monospace; font-weight: bold; font-size: 120%;}
  div.p { white-space: pre; font-family: monospace; font-weight: bold; font-size: 140%; margin-top:10px;}
  span.c { font-family: monospace; font-weight: bold; font-size: 120%;}
</style></head>
<h2>Imgcomp configuration</h2>

Imgcomp, under normal use, will run on a Raspberry Pi and launch raspistill to
capture images every second or so to a ramdisk.  Imgcomp monitors the ram disk,
comparing successive images and saving those images with changes from the previous
one to a folder hierarchy in flash.  This involves a lot of configuration options.
These are best specified in a configuration file, "imgcomp.conf" file, placed
in the current directory.
<p>
Imgcomp can also be run on a series of images stored in a folder (using the "-dodir" option),
<div class="c">imgcomp -dodir [images directory name]</div>
This option is primarily used development and diagnostics, but is also useful
for re-processing images if too many images were saved on account of setting the
different threshold too low.
<p>
Imcomp can also be run on just two images.  These images will be compared,
with debug information printed, and a difference image saved to "diff.ppm".
The diff image will be in PPM format (an uncompressed bitmap format).  This
mode is primarily used for development and diagnostics.  The invocation for this would be:
<div class="c">imgcomp [image1.jpg] [image2.jpg]</div>

<h2>Imgcomp options</h2>
The directory <b>conf-examples</b> contains a number of sample imgcomp.conf configuration files.
<p>
Imgcomp parameters can be provided on the command line or in a file "imgcomp.conf"
which is read from the current directory.
When parameters are specified in the imgcomp.conf file 
they are specified in the form:
<div class="c">parameter = value</div>
The same option, specified on the command line, would take the form:
<div class="c">-parameter value</div>
<p>
The imgcomp.conf file is read before command line arguments are interpreted.
The file "imgcomp.conf.example" contains an example imgcomp.conf file.
I recommend you start with that file.

<div class="p">aquire_cmd</div>
Specifies the command for aquiring images, including comand line optiosn to use with it.
<p>
This used to be raspistill with older
raspberry pi's, but now should be specified as libcamera-still
<p>
The "-o" option for where to put the output is appended to this command line if its not already part of it.
<p>
Also, with legacy raspistill only, the -ss and -ISO options will be appended if the exmanage option is enabled.

<div class="p">exmanage 1</div>
Set to 1 for imgcomp to manage exposure because I found raspistill's auto exposure
management too limiting.  When enabled, imgcomp analyzes the images to determine if
exposure adjustment is needed, and restarts raspistill with new ISO and shutter
speed settings when light levels change.  This feature is useful for not underexposing
images that are predominantly white, such as snow on the ground or white walls.
It also allows better compromising between grainy images and less motion blur.
this option does not work when using libcamera

<div class="p">iso &ltmin>-&ltmax></div>
With -exmanage 1, Limits of ISO settings to use with raspistill.  Must be 25-6400
 <div class="p">exposure &ltmin>-&ltmax></div>
With -exmanage 1, Limits to shutter speeds (in seconds) to use with raspistill.  0.0001-0.25
Length is limited to 0.25 because raspistill becomes unstable with long shutter
speeds in timelapse mode.

<div class="p">isooverextime</div>
with -exmanage 1, sets the ISO/shutter speed curve Unless ISO or exposure time
limits are hit.  imgcomp will aim for ISO divided by exposure time to be this value.
Default 16000.  Larger means shorter shutter speeds, smaller means lower
ISO (less grainy) but slower shutter, more motion blur.
<p>
At 16000, it will pick ISO 160 with 0.01s exposure time.<br>
With the same light level:<br>
At 64000, it will pick ISO 320 with 0.005s exposure time. (more grainy but less motion blur)<p>


<div class="p">pixsat &ltval></div>
with -exmanage 1, Jpeg pixel value at which image saturates because camera
modules v1 and v2 saturate before hitting 255.  Defaults to value appropriate
for camera module detected\n"

<div class="p">scale</div>
How much to scale jpeg images by before running the detection algorithm.  Scaling the image
down makes the program run faster and less susceptible to pixel noise.  Scale value can be
1, 2, 4 or 8.  Default is 4.  This equates to the command line option "-scale 4" or an
imgcomp.conf entry of "scale = 4". Warning, I have not tested this with non-default values.

<div class="p">region</div>
Specifies what region of the image that detection is to be done over.
The region is specified as x1-x2,y1-y2.  For example 200-600,150-450 would specify
and area in the middle, and half the size of a 800x600 image.  By default, image detection
is over the entire image.  Note that these values will be scaled down along with the image
for the actual detection algorithm, so the region edges may not be precise to the pixel.

<div class="p">exclude</div>
Specifies a rectangular image to be excluded from detection.  The region is specified
in the format x1-x2,y1-y2, similar to "region".  Multiple exclude regions can be specified.
If your detection region contains a lot
of excluded areas, I recommend using the "diffmap" option instead.

<div class="p">diffmap</div>
Specifies the name of a jpeg file that is the map of a jpeg image.  Areas colored
pure blue in the diffmap will be excluded from detection, and areas of pure red will
be twice as sensitive for detection.
<p>
Most acquired images do not contain sufficiently pure red or blue areas
to be considered "ignore" or "double", so you can take a captured image and paint
over areas to ignore (such as trees or waving flags) blue, and perhaps a doorway
in red.  When imgcomp starts with a diffmap, it shows
(in text) which areas are ignore or high weight.
<p>
Note that the diffmap is scaled down to the detection resolution, so use of the diffmap
may not be accurate to the exact pixel boundary.
<p>
<a href="diffmap.html">More on diffmaps here</a>
<p>
<div class="p">dodir</div>
Specifies which directory full of images to process.  This is for off-line mode
(not using live captures using raspistill).  Source images are not deleted.

<div class="p">followdir</div>
Specifies a directory to process and monitor for new images.  As new images appear,
imgcomp processes them and deletes them.  an "aquire_cmd" option is normally used
with this mode to launch raspistill to continuously aquire new images.

<div class="p">aquire_cmd</div>
Indicates the command line to launch raspistill with.  This line will in
turn have a lot of options to pass to raspistill.  Potentially, you
could have imgcomp run another image acquisition program instead of
raspistill.  The reason imgcomp launches raspistill is that it will need
to be able to restart raspistill if needed. Imgcomp will kill and
re-launch raspistill if it detects that image aquisition has stopped.
Imgcomp will also check if the image has become too bright or too dim
and re-launch raspistill so that raspistill can re-do the auto exposure.
The aquire_cmd parameter is only used in followdir mode (ignored in
offline "dodir" mode)

<div class="p">savedir</div>
Specifies with directory to save images to.

<div class="p">savenames</div>
Output naming scheme.  Uses strftime to format the output name.  
May include '/' characters to put different images in different directories.
The default would be savenames="%y%m%d/%H/%m%d-%H%M%S".  This value causes
imgcomp to create a directory for every day and every hour, and save the images
with the naming scheme "YYMMDD-HHMMSS".  An image on July 20 2019 at 11:50:58 would
be placed in 190720/11/0720-115058.  This name will be appended with a space and the
diff magnitude value of three digits.  In addition, if more than one image is
saved in the same second, the space in the name will be replaced with a, b, or c, etc.

<div class="p">copyjpgcmd</div>
Optional command to apply when copying jpeg image to keep.  Without this parameter,
imgcomp will just copy the file.  This parameter can be set to use the
jpegtran command to losslessly make the files from raspistill 5% smaller by setting<p>
&nbsp; &nbsp; <span class="c">copyjpgcmd = jpegtran -progressive -outfile "&o" "&i"</span><p>
However, Raspberry pi 1 and zeor are not fast enough to convert to progressive,
but on those, you can still use lesser optimization:<p>
&nbsp; &nbsp; <span class="c">copyjpgcmd = jpegtran -optimize -outfile "&o" "&i"</span><br>
<p>
Note that <span class="c">&i</span> and <span class="c">&o</span>
will be replaced with the input and output file names.


<div class="p">premotion</div>
Number of images preceeding motion to save.  Can only be 0 or 1.  Default 0.

<div class="p">postmotion</div>
Number of extra images to keep beyond one with motion.  Default 0.

<div class="p">sensitivity</div>
Indicates how sensitive the program is.  I typically use values around 100.  However,
this value is highly dependent on image resolution.  Change magnitudes peak out
around 500 because of the change magnitude is calculated over a limited size region.

<div class="p">fatigue_tc</div>
Motion fatigue time constant, default exponential decay averaging with a 30 image
time-constant.  Imgcomp will make areas with
repeated motion less senstitive to avoid triggering on things moving in the wind
such as flags or vegetation.  This feature is on by default.  Set this to zero
to turn off motion fatigue.

<div class="p">fatigue_percent</div>
Gain factor for motion fatigue strength.  Default 100.  Setting it
to 70 would cause motion fatigue effect to only be 70% as strong as by default.

<div class="p">fatigue_skip</div>
Skip applying motion fatigue every n frames.  I use this for timelapses in my workshop.
Motion fatigue will start to kick in if I am doing repetitive tasks in one location,
which is useful for saving disk space.  but I still want <i>some</i> images of repeated
motion to get saved, so I set this value to 10 to not subtract motion fatigue
for every tenth frame.  Default 0 (off)

<div class="p">spurious</div>
Set to '1' for spurious detection on, '0' for spurious detection off.  Default off.
Spurious detection ignores any changes where the images before and after an image
with a chane in it are identical.  This ignores single image events, such as an
insect flying past the camera.  Changes that do not return to the previous image
will not be ignored.

<div class="p">timelapse</div>
Specifies a time-lapse interval, in seconds.  For example "tl=600" will cause imgcomp
to save an image every ten minutes, regardless of whether motion was detected.  Time-lapse
images will occur on multiples of the specified time.  "tl=3600" will save an image on the
hour every hour, or whichever image is the first image acquired after the start of an hour.

<div class="p">logtofile</div>
Sepcifies text output to be written to a log file instead of to console.  Ideally
the log file is on a ramdisk to put less wear on flash cards.  This must be set as "/ramdisk/log.txt"
to use the Realtime display, otherwise you will see an error when accessing the Realtime view mode.

<div class="p">movelognames</div>
Where to copy logs files to.  I normally have it copy the log file to the pictures
directory every hour, so that it's easy to find the log file that goes with the pictures.

<div class="p">configfile</div>
If this is given as part of the command line parameters, it overrides the default "imgcomp.conf"
for the default options.  This is for running multiple imgcomp instances in the same user account
for multiple cameras.

<div class="p">relaunch_timeout</div>
If this is set, it replaces the default 5 seconds until imgcomp gives up on the capture
program, kills it and restarts it.  RTSP cameras with ffmpeg benefit from a longer timeout.

<div class="p">give_up_timeout</div>
If this is set, it replaces the default 15 seconds from last valid picture seen, until
imgcomp gives up and tries (assuming it has permission) to reboot the Raspberry Pi.  Highly
recommended to set if imgcomp is running on something other than a dedicated Raspi, e.g.
watching an RTSP camera as a normal process on a multiuser Linux machine.
