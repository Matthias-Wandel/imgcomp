#!/usr/bin/env python2
import socket
from struct import pack
from time import localtime, strftime,sleep
import datetime


# Encryption and Decryption of TP-Link Smart Home Protocol
# XOR Autokey Cipher with starting key = 171
def encrypt(string):
	key = 171
	result = pack('>I', len(string))
	for i in string:
		a = key ^ ord(i)
		key = a
		result += chr(a)
	return result

def decrypt(string):
	key = 171
	result = ""
	for i in string:
		a = key ^ ord(i)
		key = ord(i)
		result += chr(a)
	return result


try:
    sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_tcp.connect(("192.168.0.133", 9999))
    sock_tcp.send(encrypt('{"emeter":{"get_realtime":{}}}'))
    data = sock_tcp.recv(2048)
    sock_tcp.close()
    resstr = decrypt(data[4:])
    #print ("Received: ", resstr)
    els = resstr.split(":")
    
    current = (els[3].split(',')[0])[:6]
    volts = (els[4].split(',')[0])[:6]
    power = (els[5].split(',')[0])[:6]
    #print "voltage=",volts
    #print "current=",current
    #print "power=",power
    
    timestr = strftime("%d %b %H:%M:%S,", localtime())
    
    print timestr,volts+",",current+",",power
    
except socket.error:
	quit("Cound not connect to host ")


