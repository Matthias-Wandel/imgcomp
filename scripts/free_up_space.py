#!/usr/bin/python3
# Script to check free space on file system written to by imgcomp.
# If less then 20% free space, erase the oldes date image directory, but only one directory
# so script should be run more than once a day in case daily usage is higher than it was earlier.
import os, glob,sys
from time import localtime, strftime, time

timestr = strftime("%d-%b-%y %H:%M", localtime())

xx=os.statvfs('/')
total_blocks=xx.f_blocks
free_blocks=xx.f_bfree
percent_free = free_blocks/total_blocks*100

print(timestr+" %%%d (%3.1fG) free."%(percent_free, free_blocks/1024/1024), end='')

# If disk usage < 80%, do nothing.
if percent_free > 20:
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
    print("  Del:'",datedir, end="'")    

    now = time()
    import shutil

    shutil.rmtree(name)
    
    xx=os.statvfs('/')
    free_after=xx.f_bfree
    now2 = time()
    print(" %4.2fG freedm t=%d"%((free_after-free_blocks)/1024/1024, (now2-now)))
    
else:
    print ("No dir to delete");
    