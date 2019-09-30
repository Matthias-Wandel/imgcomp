#!/usr/bin/python3
import sys,os,glob,time
dirname = "../saved/"

dirs = os.listdir(dirname)
dirs.sort()

for o in dirs:
    if o == "keep": continue
    buckets = [0]*int(24*60/5)

    for x in os.walk(dirname+o):
        #print (x)
        for n in x[2]:
            #print (n)
            #0604-105127 0161.jpg
            if n[:3] == "Log": continue
            
            try:
                minute = int(n[5:7])*60 + int(n[7:9])
                level = int(n[12:16])
                if (level > 10):
                    buckets[int(minute/5)] += 1;
            except:
                # ignore.
                print ("ignore: ",n)
    
    OutString = ""
    for x in range(len(buckets)):
        #print (x, buckets[x])
        nc = ' '
        if x % 12 == 0: nc = '.'
        if x % 72 == 0: nc = '|'
        
        if buckets[x] > 5: nc = '-'
        if buckets[x] > 12: nc = '1'
        if buckets[x] > 40: nc = '2'
        if buckets[x] > 100: nc = '#'
        OutString = OutString + nc;
            
    print (o, OutString[72:])
