# Bug in the Raspberry Pi Zero stack
A simple program to exhibit a bug in the USB stack of the Raspberry Pi Zero.

## Sort descriptions of the bug

On random occasion, the Raspberry Pi Zero sends corrupted CRC on ``IN`` request on interrupt endpoint.

![Screenshot of the USB trace](screen_capture.png)


## Context

We produce USB 2.0 devices that work at Full Speed (not High Speed) and use two interrupt endpoints. See [http://www.yoctopuce.com]. Our devices are declared as HID devices that use a Vendor specific protocol. Our open source library use the libUSB 1.0 to communicate with our devices.

We tested and used all Raspberry Pi devices since their appartion on on market. But we discovered that official Raspbian Image after march 2016 installed on a Raspberry Pi Zero don't work with our devices any more.

After some investgations we found out that USB packet sent by the Raspberry Pi Zero have sometime Invalid CRC. According to the USB specification (section: 8.7.1) when a device receives an invalid packet, it should ignore it and the USB host (the Raspberry Pi Zero) should resend it later.

But, on instead the libUSB return an error and is no more able to use the devices. Sometime it event completely freeze the Raspberry Pi Zero.

We have not been able to determine if the issue is in the libUSB or the Linux kernel. But lots of regression testing were done and here are the results:

* Official Raspbian image until March 2016 does not have this issue.
* Only the Raspberry Pi Zero and Pi Zero W are affected.

This simple program exhibits the problem. It's a simple threaded example that iterates over all Yoctopuce connected devices and send 15 USB packets. For each sent packet the device with respond with an USB Packet.

If this binary is run on a Raspberry PI 2 or 3 everything run smoothly, but  if it is executed on a Raspberry PI Zero the libUSB report an IO error and freeze on the next packet send.


## Steps to compile this program

First clone this repository on your Raspberry PI Zero.
```
git clone https://github.com/yoctopuce-examples/raspberry_pi_zero_bug.git
```

Install libUSB 1.0 headers.

```
sudo apt-get install libusb-1.0-0-dev
```

Compile the file program with the following command

```
gcc bug_pizero.c -o bug_pizero -lusb-1.0
```

Run the example as root
```
sudo ./bug_pizero
```

### execution on a Rapberry Pi 2

```
pi@raspberrypi:~/raspberry_pi_zero_bug $ sudo ./bug_pizero
this is a simple program that exhibit a bug in the USB stack
of the Raspberry PI Zero and any Yoctopuce device
Use libUSB v1.0.19.10903
List all Yoctopuce devices present on this host:
 - USB dev: RELAYHI1-85D28 (24E0:D:0)
1 Yoctopuce devices are present
\ Test device RELAYHI1-85D28 (24E0:D)
+ Send USB pkt no 0
- rd_callback for RELAYHI1-85D28 : response 0 is valid (len=64)
+ Send USB pkt no 1
- rd_callback for RELAYHI1-85D28 : response 1 is valid (len=64)
+ Send USB pkt no 2
- rd_callback for RELAYHI1-85D28 : response 2 is valid (len=64)
+ Send USB pkt no 3
- rd_callback for RELAYHI1-85D28 : response 3 is valid (len=64)
+ Send USB pkt no 4
- rd_callback for RELAYHI1-85D28 : response 4 is valid (len=64)
+ Send USB pkt no 5
- rd_callback for RELAYHI1-85D28 : response 5 is valid (len=64)
+ Send USB pkt no 6
- rd_callback for RELAYHI1-85D28 : response 6 is valid (len=64)
+ Send USB pkt no 7
- rd_callback for RELAYHI1-85D28 : response 7 is valid (len=64)
+ Send USB pkt no 8
- rd_callback for RELAYHI1-85D28 : response 8 is valid (len=64)
+ Send USB pkt no 9
- rd_callback for RELAYHI1-85D28 : response 9 is valid (len=64)
+ Send USB pkt no 10
- rd_callback for RELAYHI1-85D28 : response 10 is valid (len=64)
+ Send USB pkt no 11
- rd_callback for RELAYHI1-85D28 : response 11 is valid (len=64)
+ Send USB pkt no 12
- rd_callback for RELAYHI1-85D28 : response 12 is valid (len=64)
+ Send USB pkt no 13
- rd_callback for RELAYHI1-85D28 : response 13 is valid (len=64)
+ Send USB pkt no 14
- rd_callback for RELAYHI1-85D28 : response 14 is valid (len=64)
= result: 15 pkt sent vs 15 pkt received
Cancel and free all pending transactions
```

### execution on a Rapberry Pi Zero


```
pi@raspberrypi:~/raspberry_pi_zero_bug $ sudo ./bug_pizero
this is a simple program that exhibit a bug in the USB stack
of the Raspberry PI Zero and any Yoctopuce device
Use libUSB v1.0.19.10903
List all Yoctopuce devices present on this host:
 - USB dev: RELAYHI1-85D28 (24E0:D:0)
1 Yoctopuce devices are present
\ Test device RELAYHI1-85D28 (24E0:D)
- need to detach kernel driver
+ Send USB pkt no 0
- rd_callback for RELAYHI1-85D28 : pkt error (len=0)
USB pkt transmit error -1 (transmitted 0 / 64)
= result: 0 pkt sent vs 0 pkt received
Cancel and free all pending transactions
```

## USB traces

The folder ``usb_traces`` contain the USB trace of two runs:

* ``usb_trace_pi_2.ufo`` : has been captured on a Raspberry PI 2
* ``usb_trace_pi_zero.ufo`` : has been captured on a Raspberry PI Zero and 	contain an packet with an invalid CRC.

These trace can be viewed with the *Visual USB Software* from Ellisys (http://www.ellisys.com/products/usbex200/download.php)


## Misc links:

* USB specifications: http://www.usb.org/developers/docs/usb20_docs/
* USB analyser used : http://www.ellisys.com/products/usbex200/index.php
* Official Raspbian images: https://downloads.raspberrypi.org/raspbian/images/
* Original project that was working with old Raspiban images:http://www.yoctopuce.com/EN/article/creating-an-multimeter-with-a-raspberry-pi-zero
