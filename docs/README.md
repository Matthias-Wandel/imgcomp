<html>
<body>
<h1>What Imgcomp does</h1>
Imgcomp makes motion triggered timelapses using a Raspberry Pi and camera module.
<p>
It serves a similar purpose as the software "motioneye", but operates on still frames
instead of video.  This makes for much better image detail, but frame rate is limited
to about 4 frames per second, depending on Raspberry Pi camera module version.
I typically use it at just 1 frame per second.
<p>
Images are initially captured to ramdisk to minimize flash wear, then analyzed and
if significant changes detected, saved to flash.
<p>
<img src="pi_and_cam.jpg">
<h1>Setting up</h1>
Setting up is done by cloning the repository onto your raspberry pi and running
the setup scripts from the <a href="../setup/">setup</a> directory.
these compile the code base and make necessary configuration changes to
crontab (for starting it) and apache2 (for browsing images)
<p>
Please see <a href="../setup/setting_up.txt">setup/setting_up.txt</a> for how to set up.
Setup scripts are in the <a href="../setup/">setup</a> directory.
<p>
Also see <a href="config.md">config.md</a> for the configuration options.

<h1>Directory structure used by imgcomp:</h1>
<table>
<tr><td>~/imgcomp/...           <td>where imgcomp source files live.
<tr><td>~/imgcomp.conf          <td>configuration file, imgcomp should run in this directory
  						        (typing imgcomp/imgcomp to run)
<tr><td>~/images/               <td>Root directory for where the images go.
<tr><td>~/images/200204/        <td>Root for pictures of Feb 4 2020 (one dir for each day,
                                named YYMMDD
<tr><td>~/images/200204/13/     <td>Pictures for 1 pm to 2 pm for Feb 4 2020
<tr><td>~/images/200204/13/0204-131445 0494.jpg&nbsp;
                                <td>Picture taken Feb 4 2020 at 13:14:45, change level 494
<tr><td>~/images/saved/         <td>Where picture are hard linked to when "saved" from image
                                browser.  One directory for each month.
<tr><td>&nbsp;

<tr><td>~/www                   <td>web root
<tr><td>~/www/view.cgi          <td>view.cgi is dropped here on compiling viewing program
<tr><td>~/www/realtime.html
<tr><td>~/www/wait_change.cgi
<tr><td>~/www/showpic.js        <td>Also dropped here on compiling viewing program                        
<tr><td>~/www/pix/              <td>links to ../../images (softlink)
<tr><td>~/www/browse.conf       <td>configuration file, used for setting local holidays
</table>
<br>
Note that imgcomp can be configured for different directory structures,
but the html browser must use the above directory structure.
<hr>
<h1>Imgcomp program</h1>

To use imgcomp, you must compile it.  
There are sample configuration files in <a href="../conf-examples/">imgcomp/conf-examples</a>
Imgcomp uses libcamera-still, raspistill or libcamera-vid to dump jpeg
files into /ramdisk (which is a ram disk the setup creates)
As images are aquired, it reads and compares these files, copies them
to ~/saved/.... if they contain relevant changes.
<p>
In the video mode hack, it looks for video files in /ramdisk/vid and uses
ffmpeg to extract stills to /ramdisk/tmp then uses those files as input.
But if you want video, look for the project "motionpi" by a different author.
<p>

<hr>

<h1>Browsing program</h1>

<b>Browsing by hour</b><br>
<img src="browse_main.jpg"><br>

The browsing program is what makes viewing the oputput from the aqusition
program fun.  You don't have to go through video clisp to see what happened.
Just hover over the bargraphs
for the times, to see what happened, clcik on the bars to navigate there,
and drag across the image left or right to scrub forward or backwards though
the qauired images.
<p>

<b>Browsing by day</b><br>
<img src="browse_day.jpg">
<p>


The browse program assumes images are stored in the www directory under
"pix", as configured by the setup script.
<p>
There is a directory for each day, therein a directory for each hour.
For example, the pictures from noon to 1 pm to 2 pm on Jun 2023 would be in<br>
&nbsp; &nbsp; &nbsp; ~/www/pix/230604/13
<p>
Files in each directory are expected to be named like this:<p>
&nbsp; &nbsp; &nbsp; 0504-130000 0002.jpg<br>
&nbsp; &nbsp; &nbsp; 0504-130809 0002.jpg<br>
&nbsp; &nbsp; &nbsp; 0504-130810 0941.jpg<br>
&nbsp; &nbsp; &nbsp; 0504-130811 2102.jpg<br>
&nbsp; &nbsp; &nbsp; .....<br>
&nbsp; &nbsp; &nbsp; Log.html<br>
<p>
Names are MMDD-HHMMSS LLLL
Where M is Month, D is Day, H is Hour, M is minute S is second, and L is 
the detected image change level.
Imgcomp creates this directory structure to store its files as it saves them.
<p>

The main viewing program is "view.cgi", which does most of the work.
There is also the "tb.cgi" which is used to generate thumbnails on the fly,
very fast using libjpeg's built in undersampling.
<p>
wait_change.cgi is used for realtime.html to wait for a significant enough
change.
<p>
showpic.js is the javascript code for flipping through the images.
<p>
Local holidays can be set in the browse.conf file in the format YYMMDD.
There is a sample configuration file in imgcomp/conf-examples.


<h1>How motion detection works</h1>
Imgcomp runs the raspberry pi camera capture program "libcamera-still" or 
"raspistill" to capture
still jpeg images every second.  With burst mode (-bm option in raspistill),
up to 4 images can be captured per second, depending on camera model.
Images could potentially come from different
sources.  I have experimented with using a webcam to capture still images, but
didn't find a sufficiently reliable still image capture solution to use with webcams.
My brother uses it with <a href="rtsp_cams.md">images captured from RTSP cameras</a>
<p>
Imgcomp tries to differentiate between important and unimportant changes.  Imgcomp
looks for localized changes in the image from one image to the next.  Imgcomp also
has the ability to ignore or emphasize specified regions during comparison
using the <a href="diffmap.md">diffmap</a> feature.
Imgcomp also has a "motion fatigue" feature that makes it ignore changes
in parts of an image that are changing a lot over time.  This feature is used to avoid
triggering too often on things swaying in the wind.




<h1>License</h1>

imgcomp is licensed under the GPL version 2 (not version 3)
For details, please see here:<br>
<a href="http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html">http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html</a>
<p>
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.
<p>
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details (docs/license.txt)
