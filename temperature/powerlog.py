#!/usr/bin/env python3
# Modified encrypt and decrypt routines to work with python3.
import socket
from struct import pack
from time import localtime, strftime,sleep
#import datetime
import json

# Encryption and Decryption of TP-Link Smart Home Protocol
# XOR Autokey Cipher with starting key = 171

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

def read_power (ip_octet):
#    try:
        sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock_tcp.connect(("192.168.0."+str(ip_octet), 9999))
        sock_tcp.send(encrypt('{"emeter":{"get_realtime":{}}}'))
        data = sock_tcp.recv(2048)
        sock_tcp.close()
        resstr = decrypt(data[4:])
        #print ("Received: ", resstr)
    
        resstr = decrypt(data[4:])
        resdict = json.loads(resstr)
        readings = resdict["emeter"]["get_realtime"]
        return readings["voltage"], readings["current"], readings["power"]
#    except:
#        return 0,0,0
    
#v,c,p = read_power(133)    
##print (v,c,p)
#print (f"133:  {v:6.2f}V {c:6.3f}A {p:6.2f}W ", end="    ")

def readtostr():
    v,c,p = read_power(113)    
    timestr = strftime("%d-%b-%y %H:%M:%S", localtime())
    return f"{timestr},{v:6.2f},{c:6.3f},{p:6.2f}"
#print (v,c,p)
log1 = readtostr()
print(log1)
sleep(2)
log2 = readtostr()
print (log2)

with open("furnace.txt", "a") as myfile:
    myfile.write(log1+"\n"+log2+"\n")



