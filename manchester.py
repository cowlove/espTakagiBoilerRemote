#!/usr/bin/python3 
import sys, argparse

parser = argparse.ArgumentParser()
parser.add_argument('-v', '--verbose', action='store_true')
parser.add_argument('file', default='-', nargs='?')
group = parser.add_mutually_exclusive_group(required=True)
group.add_argument('-d', '--dme', action='store_true')
group.add_argument('-m', '--me', action='store_true')
group.add_argument('-t', '--trans', action='store_true')
group.add_argument('-r', '--raw', action='store_true')
group.add_argument('--trace', action='store_true')
group.add_argument('--onewire', action='store_true')

parser.add_argument('-e', '--show_errors', action='store_true')
parser.add_argument('-f', '--furnace_only', action='store_true')
args = parser.parse_args()

starting = True
fpack = False
cpack = False
clock = True
bits = 0
lineno = 0
maxpacketlen = 20
timesum = 0

file = sys.stdin
if (args.file != '-'):
    file = open(args.file, "r")

for line in file:
    try:
        [v, t] = [int(num) for num in line.split()]
    except:
        continue

    if (args.trace):
        print(f"{timesum} {v} C{cpack}")
        print(f"{timesum + t} {v} C{cpack}")
    timesum = timesum + t

    if (t > 2000):
        starting = uptrans = True
        fpack = cpack = False
        clock = True
        if (args.raw):
            print ("2000 1800 CF")
            print ("2000 1800 CT")
        else:
            if bits > 0:
                print(f" [{bits}]   ", end="")
                for i in range(bits, maxpacketlen + 1): print(" ", end="")
            if t > 140000:
                print("")
        if (bits > maxpacketlen): maxpacketlen = bits
        bits = 0
        continue
    
    if (v < 1600): fpack = True
    if (v < 2000 and v > 1600 and not args.furnace_only): cpack = True
    if fpack:
        thigh = tlow = 1000
    if cpack:
        thigh = 1540
        tlow = 500
    if (cpack or fpack):
        #print(f"{v} {t} C{cpack}")
        uptrans = v < 2000
        if not uptrans:
            longpulse = t > thigh 
        else:
            longpulse = t > tlow
    
        if (args.dme):
            if longpulse and not clock and args.show_errors:
                print("\nTIMING ERROR 1")         
            if longpulse:
                print("0", end="")
                bits = bits + 1
                clock = True
            else:
                if clock:
                    print("1", end="")
                    bits = bits + 1
                clock = not clock

        if (args.me):
            if longpulse and not clock and args.show_errors:
                print("\nTIMING ERROR 1")
            if longpulse or clock:
                if uptrans: print("1", end="")
                else: print("0", end="")
                bits = bits + 1
            if longpulse:
                clock = True
            else:
                clock = not clock


        if (args.raw):
            print(f"{v} {t} C{cpack}")


        if (args.trans):
            bits = bits + 1
            if longpulse: 
                if uptrans: print ('_', end="")
                else: print("\u203e", end = "")
                bits = bits + 1
            if uptrans: print ('/', end="")
            else: print('\\', end = "")
        
        if args.onewire:
            if uptrans:
                if longpulse: print("1", end="")
                else: print("0", end="")
                bits = bits + 1

    lineno = lineno + 1
print("")