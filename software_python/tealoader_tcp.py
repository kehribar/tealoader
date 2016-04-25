#!/usr/bin/env python 
# -----------------------------------------------------------------------------
# Python version of XmegaE5 bootloader
# -----------------------------------------------------------------------------
# "THE COFFEEWARE LICENSE" (Revision 1):
# <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a coffee in return.
# -----------------------------------------------------------------------------
# Prerequisites:
# python -m pip install intelhex
# python -m pip install argparse
# python -m pip install pyserial
# -----------------------------------------------------------------------------
# ihsan Kehribar - 2016
# -----------------------------------------------------------------------------
import os
import sys
import socket
import argparse
from intelhex import IntelHex

# Variables ...
TCP_BUFFER = [0] * 512
TEALOADER_PAGE_SIZE = 128

# Single sequential main method
if __name__ == '__main__':

  if(len(sys.argv) < 3):
    print "Insufficient arguments."    
    sys.exit()

  # Sanity check
  print "File path: " + sys.argv[1]
  print "IP address: " + sys.argv[2]

  # Try to open the hex file
  try:
    ih = IntelHex()
    ih.fromfile(sys.argv[1],format='hex')
  except Exception, e:
    print "File I/O problem."
    print sys.argv[1] + " can't opened."
    sys.exit()

  # Do little analysis
  hexlen = len(ih)
  print "File length: " + str(hexlen) + " bytes."

  # Set the variables for the socket
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  server_address = (sys.argv[2], 8081)
  sock.settimeout(10);

  # Try to connect to the server
  try:
    sock.connect(server_address)  
  except Exception, e:
    print "Socket timeout."
    sys.exit();

  # Data header
  TCP_BUFFER[0] = 0xCA
  TCP_BUFFER[1] = 0xFE
  TCP_BUFFER[2] = 0xF0
  TCP_BUFFER[3] = 0x0D

  # Start the process
  TCP_BUFFER[4] = 0;
  
  try:
    sock.sendall(bytearray(TCP_BUFFER))
    data = sock.recv(4)
  except Exception, e:
    print "Socket timeout."
    sys.exit();  

  # Send the hex file gradually
  bytesSent = 0
  while bytesSent < hexlen:

    # Tranfer data to temporary buffer first
    TCP_BUFFER[4] = 1;
    for x in xrange(0,TEALOADER_PAGE_SIZE):
      if((x+bytesSent) < hexlen):
        TCP_BUFFER[5 + x] = ih[bytesSent + x]
      else:
        TCP_BUFFER[5 + x] = 0xFF      

    try:
      sock.sendall(bytearray(TCP_BUFFER))
      data = sock.recv(4)
    except Exception, e:
      print "Socket timeout."
      sys.exit();  
    
    # Then, burn it to specific offset location
    TCP_BUFFER[4] = 2;
    TCP_BUFFER[5] = (bytesSent >> 24) & 0xFF
    TCP_BUFFER[6] = (bytesSent >> 16) & 0xFF
    TCP_BUFFER[7] = (bytesSent >>  8) & 0xFF
    TCP_BUFFER[8] = (bytesSent >>  0) & 0xFF
    
    try:
      sock.sendall(bytearray(TCP_BUFFER))
      data = sock.recv(4)
    except Exception, e:
      print "Socket timeout."
      sys.exit();  

    bytesSent += TEALOADER_PAGE_SIZE      

  # Close the socket at the end
  sock.close()
  print "Socket closed."  
  print "Bootloader finished ok."
