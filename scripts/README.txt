ABOUT
=====

A script to make mp4 timelapses from output from imgcomp using ffmpeg.

This script makes use of the ffmpeg subtitling feature to add timestamps in 
the bottom right corner.


USAGE
=====

To see usage and all the available options run the following command:

```
python3 tlapse.py --help
```


```
usage: tlapse.py [-h] [-o OUTPUTFILE] [-p PATHNAME] [-k] [-n]

Make timelapses with images from imgcomp using ffmpeg.

optional arguments:
  -h, --help            show this help message and exit
  -o OUTPUTFILE, --outputfile OUTPUTFILE
                        Output file name.
  -p PATHNAME, --pathname PATHNAME
                        Path to a directory containing imgcomp images. Default
                        is current dir.
  -k, --keep-assets     Flag to keep both the pic list and timestamps header
                        files.
  -n, --no-timestamp    Flag to disable timestamps in timelapse output video.
```
