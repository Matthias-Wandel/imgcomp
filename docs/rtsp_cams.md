<html>
<head>
<title>Imgcomp RTSP camera documentation</title>
<style type=text/css>
  body { font-family: sans-serif; font-size: 100%;}
  div.c { white-space: pre; font-family: monospace; font-weight: bold; font-size: 120%;}
  span.c { font-family: monospace; font-weight: bold; font-size: 120%;}
</style></head>

<h2>Why RTSP cameras?</h2>
What is here called an RTSP camera is any camera that claims to be "ONVIF" compliant.
Generally these use Hikvision chipsets, and can only be configured using an ActiveX plugin
running on an obsolete Windows browser and only accessed by pulling a H.264 stream via
the RTSP protocol.  They're meant to be used with continuously recording surveillance
boxes.
<p>
But they're also cheap.  At the time of this writing (April 2020) it is possible to
obtain a 1080p, wide-angle (2.8mm lens, approximately 90 degree angle of vision horizontally)
PoE powered, weatherproof camera for about Can$35 directly from China via Ali Express.
The picture quality is not as good as that of a Raspicam, but you don't need one Raspi
per camera; one Raspi3 can easily handle 3 or 4 of them without even a heat sink.
<h2>What this does and does not do</h2>
This is a direct drop-in replacement for a Raspicam.  Still pictures are captured at
a lowish rate (1-2 per second) and fed into imgcomp's normal processing.  There is no
provision for capturing continous video, or at this time even for the experimental
video clip option of imgcomp.
<h2>How to set it up</h2>
First of all, make sure the camera can be accessed over the network.  A completely
default Hivision camera generally has a static IP address of 192.168.1.10.  To reconfigure
that you need to at least temporarily set up a 192.168.1.x network, then access the camera
from a Windows machine with a browser that can run ActiveX (and has all the appropriate security
overridden to run a grotty binary plugin from who knows where).  Once all that works, go
and set the camera to the IP address you want, and under "NetService" turm off "Cloud" - you
don't want to send your video off to China.  All in all, best to keep these cameras on a
subnet that does not have internet access anyway.
Once the camera works, you can just open it as a netwok stream in VLC using an URL like
<p>
rtsp://IP_ADDRESS:554/user=admin&password=&channel=1&stream=0.sdp
<p>
If you see video, you're set.  Now you need to replace the line "aquire_cmd" with the 
following.
<p>
aquire_cmd = ffmpeg -rtsp_transport tcp -i rtsp://IP_ADDRESS:554/user=admin&password=&channel=1&stream=0.sdp -q:v 1 -r 2 /tmp/in2/%06d.jpg<br>
relaunch_timeout = 30<br>
give_up_timeout = 0<br>
<p>
The options are explained in the config section.
<h2>Multiple Cameras</h2>
Given that a Raspi can easily handle multiple cameras of this type, support is provided
for running multiple ones on the same device easily.
<p>
For one thing, imgcomp can be started with<p>
-configfile FILENAME
<p>
which will override "imgcomp.conf" as the default configuration.  This way several can be run from the same
working directory.
<p>
Also, in the "www" directory, you can make subdirectories for each camera, with symlinks as follows:
<p>
favicon.ico -> ../favicon.ico<br>
realtime.html -> ../realtime.html<br>
showpic.js -> ../showpic.js<br>
tb.cgi -> ../tb.cgi<br>
view.cgi -> ../view.cgi<br>
wait_change.cgi -> ../wait_change.cgi<br>
<p>
This way, if the stuff in the "browse" directory is recompiled, all the camera directories see it.  Each
camera directory has the additional links
<p>
in -> wherever ffmpeg or rapsistill is dumping its output<br>
pix -> wherever imgcomp is saving its output<br>
<p>
The "pix" link is required for view.cgi to work.  If the "in" link is missing, it defaults to "/ramdisk"
which is imgcomp's hardwired default.
<h2>Transferring Pictures Off the Raspi</h2>
To avoid SD card wear, it is possible to dump imgcomp's output in the RAM disk.  However this requires
an ancillary script that constantly moves things off the RAM disk to elsewhere on the network.  Such a
script (which needs customization) is provided the conf-examples directory.
