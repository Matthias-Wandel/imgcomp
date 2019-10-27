#!/usr/bin/python3
#
# Requires Adafruit DHT library.  Run this to get it:
#    pip3 install Adafruit_DHT

import Adafruit_DHT
import time
 
DHT_SENSOR = Adafruit_DHT.DHT22
DHT_PIN = 15


def ReadRetry(pin):
    humidity, temperature = Adafruit_DHT.read(DHT_SENSOR, DHT_PIN)
    if humidity is None or temperature is None or humidity > 100 or humidity < 10 or temperature < -25 or temperature > 45:
        # Try again.
        humidity, temperature = Adafruit_DHT.read(DHT_SENSOR, pin)
    if humidity is None or temperature is None or humidity > 100 or humidity < 10 or temperature < -25 or temperature > 45:
        # Try again.
        humidity, temperature = Adafruit_DHT.read(DHT_SENSOR, pin)
    return humidity, temperature


h14,t14 = ReadRetry(14)
h15,t15 = ReadRetry(15)
logstr = time.strftime("%d-%b-%y %H:%M:%S, ", time.localtime())
logstr = logstr + "{0:0.1f},{1:0.1f},{2:0.1f},{2:0.1f}\n".format(h14,h15,t14,t15)
print(logstr)

with open("/home/pi/templog2.txt", "a") as myfile:
        myfile.write(logstr)
