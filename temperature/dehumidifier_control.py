#!/usr/bin/python3
import Adafruit_DHT
from time import localtime, strftime,sleep
import datetime

import socket
from struct import pack
import json


# Sensor should be set to Adafruit_DHT.DHT11,
# Adafruit_DHT.DHT22, or Adafruit_DHT.AM2302.
sensor = Adafruit_DHT.DHT22
pin = 14

def do_temp():
    # Read the sensor
    humidity, temperature = Adafruit_DHT.read_retry(sensor, pin)
    if humidity is None or temperature is None or humidity > 80 or humidity < 25 or temperature > 30 or temperature < 0:
        # retry, but only once.
        humidity, temperature = Adafruit_DHT.read_retry(sensor, pin)
    if humidity is None or temperature is None or humidity > 80 or humidity < 25 or temperature > 30 or temperature < 0:
        # retry maybe a second time!
        humidity, temperature = Adafruit_DHT.read_retry(sensor, pin)
	
    if humidity is not None and temperature is not None:
        print('Temperature={0:0.1f}  Humidity={1:0.1f}'.format(temperature, humidity))
        logstr = '{0:0.1f}, {1:0.1f}'.format(temperature, humidity)
    else:
        # Its possible there won't be a reading.
        logstr = "no reading\n"

    logstr = strftime("%d-%b-%y %H:%M:%S, ", localtime()) + logstr
        
    return logstr, humidity, temperature


def encrypt(string):
    key = 171
    result = bytearray(pack('>I', len(string)))
    for i in string:
        a = key ^ ord(i)
        key = a
        result.append(a)
    return result

def decrypt(string):
    key = 171
    result = ""
    bstring = bytes(string)
    for i in bstring:
        a = key ^ i
        key = i
        result += chr(a)
    return result

def onoff(ip_octet, on_off):
    try:
        sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_tcp.connect(("192.168.0."+ip_octet, 9999))
        sock_tcp.send(encrypt('{"system":{"set_relay_state":{"state":'+on_off+'}}}'))
        data = sock_tcp.recv(2048)
        sock_tcp.close()
        resstr = decrypt(data[4:])
        resdict = json.loads(resstr)
        print (resdict["system"]["set_relay_state"])
        rc = str(resdict["system"]["set_relay_state"]["err_code"])
        if rc == "0": rc = "ok"
        return rc
   
    except socket.error:
        print("Cound not connect to tp-link")
        return "error"





logstr, humidity, temp = do_temp();
# Do two readings about 30 seconds apart
#sleep(25)
#do_temp();

ip_octet = "140"


if humidity > 55:
    res = onoff(ip_octet, "1")
    logstr += ", 1, "+res
    
if humidity < 53:
    res = onoff(ip_octet, "0")
    logstr += ", 0, "+res

logstr = logstr + "\n"
print (logstr)
with open("/home/pi/templog.txt", "a") as myfile:
    myfile.write(logstr)

