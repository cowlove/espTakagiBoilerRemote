#!/usr/bin/python3 
size = 2500
arr = [[0]*size]*size

starting = True
fpack = False
cpack = False

with open("x.dat", "r") as file: 
    for line in file:
        [v, t] = [int(num) for num in line.split()]
        if (t > 2000):
            starting = True
            fpack = False
            cpack = False
            print("2300 1800 PACKET C F")
            continue
        if (v < 1600): fpack = True
        if (v < 2000 and v > 1600): cpack = True
        if (fpack == True):
            print(f"{v} {t} 0 F")
        if (cpack == True):
            print(f"{v} {t} 0 C")
           
