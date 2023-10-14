# midigw

## Introduction
This is a HTTP to MIDI over serial/USB gateway originally developed for usage on a Raspberry Pi.

## Dependencies
- CMake
- gcc 10
- Python 3
- Flask `pip install flask`
- getopt `pip install getopt`

## Build
After cloning, do the following steps to build
```
$ mkdir -p build
$ cd build
$ cmake ..
$ make
```

## Install
After a successful build, do this to install the tapper binary and the gateway script
```
$ sudo make install
```
This will copy binaries and scripts to `$PREFIX/bin` (default `/usr/local/bin`)

If you want to setup the gateway as a systemd service, do this:
```
$ sudo cp config/midigw.service /etc/systemd/system/
$ sudo systemctl enable midigw
$ sudo systemctl start midigw
```
