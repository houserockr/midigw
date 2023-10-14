#!/usr/bin/env python3

from flask import Flask
from http.client import responses
import serial

from subprocess import Popen, PIPE
import signal
import sys
import os
import time
import getopt
from stat import *

app = Flask(__name__)

# path to serial port to send midi message to
serial_dev = "/dev/serial0"
# path to the midi tapper program
tap_proc_cmd = "/usr/local/bin/midi-tapper"
# "pointer" to running tapper proc
tap_proc = None

def send_serial(data):
    ser = serial.Serial(serial_dev, 31250)
    ser.write(data)

def send_cc_serial(channel, message, value):
    msg_bytes = bytes([
        (0xB << 4) | channel,
        message, value])
    send_serial(msg_bytes)

def render_err(code: int, message: str):
    err_str = responses[code]
    msg = "<h1>{} {}</h1><p>{}</p>".format(
            code, err_str, message)
    return msg

def start_tap_proc(channel, message, value, bpm):
    global tap_proc
    cmd = [tap_proc_cmd,
            "-s", serial_dev,
            "-c", channel,
            "-m", message,
            "-v", value,
            "-t", bpm]
    tap_proc = Popen(cmd, stdout=PIPE, stderr=PIPE)

def stop_tap_proc():
    global tap_proc
    tap_proc.send_signal(signal.SIGINT)
    time.sleep(1)
    tap_proc.send_signal(signal.SIGTERM)
    tap_proc = None

@app.route("/midi/tap/start/<channel>/<message>/<value>/<bpm>")
def midi_tap_start(channel, message, value, bpm):
    start_tap_proc(channel, message, value, bpm)
    return "OK", 200

@app.route("/midi/tap/stop")
def midi_tap_stop():
    if tap_proc == None:
        return "Not running", 400
    stop_tap_proc()
    return "OK", 200

@app.route("/midi/cc/<int:channel>/<int:message>/<int:value>")
def midi_cc(channel, message, value):
    send_cc_serial(channel, message, value)
    return "OK", 200

@app.route("/ping")
def ping():
    return "We're alive.", 200

def check_deps():
    if not S_ISCHR(os.stat(serial_dev).st_mode):
        print("{} is not a character device. Exiting.".format(serial_dev))
        sys.exit(1)

    if not os.path.exists(tap_proc_cmd):
        print("{} does not exist. Exiting.".format(tap_proc_cmd))
        sys.exit(1)

    mode = os.stat(tap_proc_cmd).st_mode
    if not (mode & S_IXUSR or mode & S_IXGRP or mode & S_IXOTH):
        print("{} not executable. Exiting.".format(tap_proc_cmd))
        sys.exit(1)

def usage():
    s = """
Usage: {} [-h/-p/-i/-s/-t]
-h --help        display this help
-p --port <>     port to listen for connections (default 5000)
-i --ip <>       ip address to bind to (default 0.0.0.0)
-s --serial <>   serial port to write to (default /dev/serial0)
-t --tap-proc <> path to the serial midi tapper program (default /usr/local/bin/midi-tapper)
""".format(sys.argv[0])
    print(s)

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hp:i:s:t:", ["help", "port=", "ip=", "serial=", "tap-proc="])
    except getopt.GetoptError as err:
        print(err)
        usage()
        sys.exit(2)
    bind_ip = "0.0.0.0"
    bind_port = 5000
    global serial_dev
    global tap_proc_cmd
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-p", "--port"):
            bind_port = a
        elif o in ("-i", "--ip"):
            bind_ip = a
        elif o in ("-s", "--serial"):
            serial_dev = a
        elif o in ("-t", "--tap-proc"):
            tap_proc_cmd = a
        else:
            assert False, "unhandled option"

    check_deps() # will exit on error
    print("Using serial port {}".format(serial_dev))
    print("Using tapper proc {}".format(tap_proc_cmd))
    app.run(host=bind_ip, port=int(bind_port))

if __name__ == "__main__":
    main()
