#!/usr/bin/env python

import sys
import struct

if __name__ == '__main__':
	if len(sys.argv) < 2:
		print "usage: %s <image>" % sys.argv[0]
		sys.exit(0)
	
	h = open(sys.argv[1], "r")

	magic = h.read(8)
	if magic != "\x88\x88\x88\x88\x63\x6F\x7A\x79":
		print "Error: didn't find magic number"
		sys.exit(1)

	crc = h.read(4)
	gen_level = h.read(4)
	backend_version = h.read(4)
	version = h.read(4)
	entry = h.read(32)
	while len(entry) == 32:
		if entry[0] == '\0':
			print "(terminating entry)"
			break
		(offset,size) = struct.unpack('ii', entry[24:])
		l = 0
		while l < 24 and entry[l] != '\0':
			l += 1
		print "\"%s\"%s\toffset: 0x%X\t\tlength: 0x%X\t(%d)" % \
				(entry[0:23], ' '*(24-l), offset, size, size)
		entry = h.read(32)
	
	h.close()
	sys.exit(0)
