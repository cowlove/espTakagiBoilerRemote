#!/usr/bin/python3 
import sys
starting = True
fpack = False
cpack = False
clock = True
tlow = 0
thigh = 0
bits = 0
uptrans = True
lineno = 0
for line in sys.stdin:
    [v, t] = [int(num) for num in line.split()]
    if (t > 2000):
        starting = uptrans = True
        fpack = cpack = False
        clock = False
        print ("2000 1800 CF")
        print ("2000 1800 CT")
        #print(f" [{bits}]   ", end="")
        #for i in range(bits, 30): print(" ", end="")
        #if bits < 20:
        #    print("")
        bits = 0

        #continue
    if (v < 1600): fpack = True
    if (v < 2000 and v > 1600): cpack = True
    if fpack:
        thigh = tlow = 1000
    if cpack:
        thigh = 1540
        tlow = 1100
    if (cpack or fpack):
        print(f"{v} {t} C{cpack}")
        if (uptrans and v > 2000) or (not uptrans and v < 2000): print ("TIMING ERROR 1") 
        uptrans = v > 2000
        if uptrans:
            if (t < thigh): 
                clock = not clock
            else:
                if not clock: print (f"TIMING ERROR 3 {uptrans} {fpack} {cpack} {lineno}")
                clock = True
        else:
            if (t < tlow): 
                clock = not clock
            else:
                if not clock: print (f"TIMING ERROR 3 {uptrans} {fpack} {cpack} {lineno}")
                clock = True
        if False and clock:
            bits = bits + 1
            if uptrans: 
                print("1", end="")
            else:
                print("0", end="")
    lineno = lineno + 1