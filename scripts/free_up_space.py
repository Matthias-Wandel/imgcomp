#!/usr/bin/python3
# Script to check free space on file system written to by imgcomp.
# If less then 20% free space, erase the oldest date image directory, but only one directory
# so script should be run more than once a day in case daily usage is higher than it was earlier.
#
# Crontab entry to run it every 6 hours, 7 minutes into the hour.
# 7 0,6,12,18 * * * /home/pi/imgcomp/scripts/free_up_space.py >> /home/pi/freelog.txt

import os, glob,sys
from time import localtime, strftime, time

timestr = strftime("%d-%b-%y %H:%M", localtime())

xx=os.statvfs('/home/pi/images')
total_blocks=xx.f_blocks*xx.f_bsize/1024
avail_blocks=xx.f_bavail*xx.f_bsize/1024
percent_avail = avail_blocks/total_blocks*100

print(timestr+" %d%% (%3.1fG) free."%(percent_avail, avail_blocks/1024/1024), end='')

# If disk usage < 80%, do nothing.
if percent_avail > 20:
    print()
    sys.exit();

dirs = glob.glob("/home/pi/images/??????/")
dirs.sort()

wipedir = ""
for name in dirs:
    segs = name.split("/")
    if len(segs) < 3: continue
    datedir = segs[-2]
    if not datedir.isdigit(): continue
    if len(datedir) != 6: continue
    wipedir = name
    break

if wipedir:
    print("  Del:'"+datedir, end="'")    

    now = time()
    import shutil

    shutil.rmtree(name)
    
    xx=os.statvfs('/')
    avail_after=xx.f_bavail*xx.f_bsize/1024
    now2 = time()
    print(" %4.2fG freed: t=%d"%((avail_after-avail_blocks)/1024/1024, (now2-now)))
    
else:
    print ("No dir to delete");
    