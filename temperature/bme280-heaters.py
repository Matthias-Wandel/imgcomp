#!/usr/bin/python3
#  From bme280.py by Matt Hawkins
#  https://www.bosch-sensortec.com/bst/products/all_products/bme280
import smbus
import time
from ctypes import c_short
from ctypes import c_byte
from ctypes import c_ubyte

import socket
from struct import pack
#import json

DEVICE = 0x76 # Default device I2C address


bus = smbus.SMBus(1) # Rev 2 Pi, Pi 2 & Pi 3 uses bus 1
                     # Rev 1 Pi uses bus 0

def getShort(data, index):
  # return two bytes from data as a signed 16-bit value
  return c_short((data[index+1] << 8) + data[index]).value

def getUShort(data, index):
  # return two bytes from data as an unsigned 16-bit value
  return (data[index+1] << 8) + data[index]

def getChar(data,index):
  # return one byte from data as a signed char
  result = data[index]
  if result > 127:
    result -= 256
  return result

def getUChar(data,index):
  # return one byte from data as an unsigned char
  result =  data[index] & 0xFF
  return result

def readBME280ID(addr=DEVICE):
  # Chip ID Register Address
  REG_ID     = 0xD0
  (chip_id, chip_version) = bus.read_i2c_block_data(addr, REG_ID, 2)
  return (chip_id, chip_version)

def readBME280All(addr=DEVICE):
  # Register Addresses
  REG_DATA = 0xF7
  REG_CONTROL = 0xF4
  REG_CONFIG  = 0xF5

  REG_CONTROL_HUM = 0xF2
  REG_HUM_MSB = 0xFD
  REG_HUM_LSB = 0xFE

  # Oversample setting - page 27
  OVERSAMPLE_TEMP = 2
  OVERSAMPLE_PRES = 2
  MODE = 1

  # Oversample setting for humidity register - page 26
  OVERSAMPLE_HUM = 2
  bus.write_byte_data(addr, REG_CONTROL_HUM, OVERSAMPLE_HUM)

  control = OVERSAMPLE_TEMP<<5 | OVERSAMPLE_PRES<<2 | MODE
  bus.write_byte_data(addr, REG_CONTROL, control)

  # Read blocks of calibration data from EEPROM
  # See Page 22 data sheet
  cal1 = bus.read_i2c_block_data(addr, 0x88, 24)
  cal2 = bus.read_i2c_block_data(addr, 0xA1, 1)
  cal3 = bus.read_i2c_block_data(addr, 0xE1, 7)

  # Convert byte data to word values
  dig_T1 = getUShort(cal1, 0)
  dig_T2 = getShort(cal1, 2)
  dig_T3 = getShort(cal1, 4)

  dig_P1 = getUShort(cal1, 6)
  dig_P2 = getShort(cal1, 8)
  dig_P3 = getShort(cal1, 10)
  dig_P4 = getShort(cal1, 12)
  dig_P5 = getShort(cal1, 14)
  dig_P6 = getShort(cal1, 16)
  dig_P7 = getShort(cal1, 18)
  dig_P8 = getShort(cal1, 20)
  dig_P9 = getShort(cal1, 22)

  dig_H1 = getUChar(cal2, 0)
  dig_H2 = getShort(cal3, 0)
  dig_H3 = getUChar(cal3, 2)

  dig_H4 = getChar(cal3, 3)
  dig_H4 = (dig_H4 << 24) >> 20
  dig_H4 = dig_H4 | (getChar(cal3, 4) & 0x0F)

  dig_H5 = getChar(cal3, 5)
  dig_H5 = (dig_H5 << 24) >> 20
  dig_H5 = dig_H5 | (getUChar(cal3, 4) >> 4 & 0x0F)

  dig_H6 = getChar(cal3, 6)

  # Wait in ms (Datasheet Appendix B: Measurement time and current calculation)
  wait_time = 1.25 + (2.3 * OVERSAMPLE_TEMP) + ((2.3 * OVERSAMPLE_PRES) + 0.575) + ((2.3 * OVERSAMPLE_HUM)+0.575)
  time.sleep(wait_time/1000)  # Wait the required time  

  # Read temperature/pressure/humidity
  data = bus.read_i2c_block_data(addr, REG_DATA, 8)
  pres_raw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4)
  temp_raw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4)
  hum_raw = (data[6] << 8) | data[7]

  #Refine temperature
  var1 = ((((temp_raw>>3)-(dig_T1<<1)))*(dig_T2)) >> 11
  var2 = (((((temp_raw>>4) - (dig_T1)) * ((temp_raw>>4) - (dig_T1))) >> 12) * (dig_T3)) >> 14
  t_fine = var1+var2
  temperature = float(((t_fine * 5) + 128) >> 8);

  # Refine pressure and adjust for temperature
  var1 = t_fine / 2.0 - 64000.0
  var2 = var1 * var1 * dig_P6 / 32768.0
  var2 = var2 + var1 * dig_P5 * 2.0
  var2 = var2 / 4.0 + dig_P4 * 65536.0
  var1 = (dig_P3 * var1 * var1 / 524288.0 + dig_P2 * var1) / 524288.0
  var1 = (1.0 + var1 / 32768.0) * dig_P1
  if var1 == 0:
    pressure=0
  else:
    pressure = 1048576.0 - pres_raw
    pressure = ((pressure - var2 / 4096.0) * 6250.0) / var1
    var1 = dig_P9 * pressure * pressure / 2147483648.0
    var2 = pressure * dig_P8 / 32768.0
    pressure = pressure + (var1 + var2 + dig_P7) / 16.0

  # Refine humidity
  humidity = t_fine - 76800.0
  humidity = (hum_raw - (dig_H4 * 64.0 + dig_H5 / 16384.0 * humidity)) * (dig_H2 / 65536.0 * (1.0 + dig_H6 / 67108864.0 * humidity * (1.0 + dig_H3 / 67108864.0 * humidity)))
  humidity = humidity * (1.0 - dig_H1 * humidity / 524288.0)
  if humidity > 100:
    humidity = 100
  elif humidity < 0:
    humidity = 0

  return temperature/100.0,pressure/100.0,humidity

def ReadBmeAll():

  (chip_id, chip_version) = readBME280ID()
  print ("Chip ID     :", chip_id)
  print ("Version     :", chip_version)

  temperature,pressure,humidity = readBME280All()

  print ("Temperature : ", temperature, "C")
  print ("Pressure : ", pressure, "hPa")
  print ("Humidity : ", humidity, "%")



#==========================================================================================================================

def TpLinkOnOff(ip_octet, on_off):
    if on_off == False: on_off = 0
    if on_off == True: on_off = 1
    print("Set switch ",ip_octet," to",on_off)

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


    try:
        sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_tcp.connect(("192.168.0."+str(ip_octet), 9999))
        sock_tcp.send(encrypt('{"system":{"set_relay_state":{"state":'+str(on_off)+'}}}'))
        data = sock_tcp.recv(2048)
        sock_tcp.close()
        resstr = decrypt(data[4:])
        print (resstr)
        #resdict = json.loads(resstr)
        #print (resdict["system"]["set_relay_state"])
        #rc = str(resdict["system"]["set_relay_state"]["err_code"])
        #if rc == "0": rc = "ok"
        #return rc
   
    except socket.error:
        print("Cound not connect to tp-link")
        return "error"

#==========================================================================================================================


def TempFromSched(schedule, hour):
    shh = -1;
    index = 0;
    while (shh < hour):
        if index+1 >= len(schedule): break;
        if schedule[index+1][0] >= hour: break
        index += 1
    return schedule[index][1]

tm = time.localtime()
hour = tm.tm_hour + tm.tm_min/60
    
#Weekday schedule
kidsroom_schedule = [[0,17.5],[7.5,13],[15,15.5],[16,16.5],[18,17.5]]
toyroom_schedule = [[0,16.5],[3,18],[4,19],[5,20],[6,21],[7.5,16],[15.5,19.5],[16.5,21],[19.75,16.5]]

#print ("week day is",tm.tm_wday, tm.tm_mday)
if tm.tm_wday >= 5 and tm.tm_mday != 5:
    #Saturday=5, sunday=6
    kidsroom_schedule = [[0,17.5],[17.75,17.5]]
    toyroom_schedule = [[0,16],[4,18],[5.5,19],[6,21],[18,21],[19.5,16.5]]
    if tm.tm_wday == 6: # Sunday  Cool down early cause dinner at Rachel's parents.
        toyroom_schedule = [[0,16],[4,18],[5.5,19],[6,21],[17,16.5]]

# Special schedule for just now.        
if tm.tm_mday == 19:
    toyroom_schedule = [[0,16.5],[3,18],[4.5,19],[5,20],[6,21],[8,16],[17.5,19.5],[18,21],[19.75,16.5]]



# Use try catch to still log the other sensor in case one of them fails.

# Kids bedrooms
TargetTempK = TempFromSched(kidsroom_schedule, hour)
try:
    temp6,press6,humid6 = readBME280All(0x76)
    tdiff = temp6 - TargetTempK

    on1 = 1; on2 = 1
    if tdiff > 0.2:  on1 = 0
    if tdiff > -0.2: on2 = 0
    
    TpLinkOnOff(101, on1)
    TpLinkOnOff(188, on2)
except:
    temp6=press6=humid6 = float("nan")

# Toyroom
TargetTempT = TempFromSched(toyroom_schedule, hour)
#try:
if 1:
    temp7,press7,humid7 = readBME280All(0x77)
    tdiff = temp7 - TargetTempT
    
    on1 = 1; on2 = 1
    if tdiff > -0.3:on1 = 0; on2 = 1
    if tdiff > 0:   on1 = 1; on2 = 0
    if tdiff > 0.3: on1 = 0; on2 = 0
    TpLinkOnOff(104, on1) # was 150
    TpLinkOnOff(100, on2) # was 133
    
#except:
#    temp7 = press7 = humid7 = float("nan")


timestr = time.strftime("%d-%b-%y %H:%M:%S, ", tm)
logstr = timestr + format(temp6, "^-2.2f") + ", "+ format(temp7, "^-2.2f")
logstr = logstr + ", " + format(humid6, "^-2.1f") + ", "+ format(humid7, "^-2.1f")
logstr = logstr + ", " + format(TargetTempK, "^-2.1f")+ ", " + format(TargetTempT, "^-2.1f") + ", " + format(press6, "^-2.1f") + "\n"
print (logstr)

with open("/home/pi/upstairs.txt", "a") as myfile:
    myfile.write(logstr)


