#!/usr/bin/python3
import Adafruit_DHT
from time import localtime, strftime,sleep
import datetime

# Sensor should be set to Adafruit_DHT.DHT11,
# Adafruit_DHT.DHT22, or Adafruit_DHT.AM2302.
sensor = Adafruit_DHT.DHT22
pin = 14

def do_temp():
    # Read the sensor
    humidity, temperature = Adafruit_DHT.read_retry(sensor, pin)
    if humidity is None or temperature is None:
        # retry, but only once.
        humidity, temperature = Adafruit_DHT.read_retry(sensor, pin)
	
    if humidity is not None and temperature is not None:
        print('Temperature={0:0.1f}  Humidity={1:0.1f}'.format(temperature, humidity))
        logstr = '{0:0.1f}, {1:0.1f}\n'.format(temperature, humidity)
    else:
        # Its possible there won't be a reading.
        logstr = "no reading\n"
	
    logstr = strftime("%d-%b-%y %H:%M:%S, ", localtime()) + logstr
    print (logstr)
	
    # Log it into two files, in case I accidentally wipe out the log.
#    with open("/home/pi/biglog.txt", "a") as myfile:
#        myfile.write(logstr)

    with open("/home/pi/templog.txt", "a") as myfile:
        myfile.write(logstr)

do_temp();
# Do two readings about 30 seconds apart
#sleep(25)
#do_temp();


