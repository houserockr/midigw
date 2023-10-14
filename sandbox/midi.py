#!/usr/bin/env python3

import serial
import signal
import sys
import ctypes

libc = ctypes.CDLL('libc.so.6')
ser = serial.Serial(sys.argv[1], 31250)

def signal_handler(sig, frame):
    ser.close()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

while True:
    ser.write(b'\xb0\x2c\x00')
    libc.usleep(526316)



