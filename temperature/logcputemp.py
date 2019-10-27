#!/usr/bin/python3
# Take several CPU temperature readings to get a good average.
import time, datetime, subprocess

timestr = ""
temp_t = 0

for x in range(10):
    result = subprocess.run(["vcgencmd", "measure_temp"], stdout=subprocess.PIPE)
    out = str(result.stdout)
    temp = float(out.split("temp=")[1][:4])
    print(temp)
    if x == 4: # Two minutes in.
        timestr = time.strftime("%d-%b-%y %H:%M:%S,", time.localtime())
        print (timestr)
    
    temp_t += temp
    if x < 10: time.sleep(30)

logstr = timestr + format(temp_t/10, "^-2.1f") + "\n"
print (logstr)

with open("/home/pi/templog.txt", "a") as myfile:
    myfile.write(logstr)




