
Setting up imgcomp
Please see setup/setting_up.txt for instructions on 
which scripts to run to set up.

------------------------------------------------------------------------------

Assumed directory structure:
~/imgcomp/...           where imgcomp source files live.
~/imgcomp.conf          config file, imgcomp should run in this directory (typing imgcomp/imgcomp to run)
~/images/               Root directory for where the images go.
~/images/200204/        Root for pictures of Feb 4 2020 (one dir for each day, named YYMMDD
~/images/200204/13/     Pictures for 1 pm to 2 pm for Feb 4 2020
~/images/200204/13/0204-131445 0494.jpg
                        Picture taken Feb 4 2020 at 13:14:45, change level 494
~/images/saved/         Where picture are hard linked to when "saved" from image
                        browser.  One directory for each month.

~/www                   web root
~/www/view.cgi          view.cgi is dropped here on compiling viewing program
~/www/realtime.html
~/www/wait_change.cgi
~/www/showpic.js        Also dropped here on compiling viewing program                        
~/www/pix/              Root of the pictures (a soft link to ~/images)

Note that imgcomp can be configured for different directory structures,
but the html browser must use the above directory structure.

------------------------------------------------------------------------------

IMGCOMP PROGRAM
To use imgcomp, you must compile it.  
After unpacking it on a Raspberry Pi, type "make" in the imgcomp directory.
There are sample configuration files in imgcomp/conf-examples
Imgcomp uses raspistill to dump files into /ramdisk (which is a ram disk)
Then reads and compares these files, copies them to ~/saved/.... if they
contain relevant changes.  In the video mode hack, it looks for video files
in /ramdisk/vid and uses ffmpeg to extract stills to /ramdisk/tmp
then uses those files as input.

Imgcomp uses the EXIF date in the file to figure out when the photos were
taken (not sure offhand if it will fall back to file date or current
date/time if there is no EXIF date in the pictures.

There is the imgcomp/setup directory which contains scripts that I use to
configure it to run on a raspberry pi, plus some scripts that are only
useful to me.  Also read file "imgcomp/setup/setting_up.txt"

------------------------------------------------------------------------------

BROWSING PROGRAM
To compile the web still image browser, go into imgcomp/browse and type
"make". The makefile automatically copies the compiled CGI programs into
the directory "../../www". It sets the "setuid bit" for view.cgi so that
it runs as user "pi".  Thus when "saving" files to the saved directory,
Linux file ownership remains consistent.

The browse program assumes images are stored in the www directory under
"pix", with a directory for each day, therein a directory for each hour.
For example, the pictures from noon to 1 pm on Jan 4 2020 would be in
    ~/www/pix/200104/13

Files in each directory are expected to be named:
    0104-130000 0002.jpg
    0104-130809 0002.jpg
    0104-130810 0941.jpg
    0104-130811 2102.jpg
    .....
    Log.html
    
First four digits are date of the year (excluding year)
Next six digits are time of day
Last four digits are change level.
Log.html is a log file that the browse tool can also display.
Imgcomp creates this directory structure to store its files as it saves them.

The main viewing program is "view.cgi", which does most of the work.
There is also the "tb.cgi" which is used to generate thumbnails on the fly,
very fast using libjpeg's built in undersampling.

wait_change.cgi is used for realtime to wait for a significant enough change.
showpic.js is the javascript code for flipping through the images.


------------------------------------------------------------------------------

There is also the "docs" directory containing more documents.

------------------------------------------------------------------------------

imgcomp is licensed under the GPL version 2 (not version 3)
For details, please see here: 
http://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details (docs/license.txt)
