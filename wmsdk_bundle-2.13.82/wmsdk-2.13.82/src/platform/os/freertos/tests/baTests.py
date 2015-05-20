import unittest
import pexpect
import time
import cfg

global config
global mtfterm

class TestBLOCKALLOC(unittest.TestCase):
	if cfg.CONFIG_OS_FREERTOS:
		def test(self):
			expectedLine = "Success"
#before sending command to the device,used fuction readlines()to flush all the old data from buffer
			mtfterm.readlines();
			mtfterm.sendline("slab-alloc-test");
			failMessage = "Error"
			line = mtfterm.readline();
			previous_line = line
			while line != "":
				previous_line = line
			  	if line == expectedLine:
					break;
				line = mtfterm.readline()
			failMessage = "Expected '%s' got '%s'" % (expectedLine, previous_line)
			self.failIf(expectedLine != previous_line, failMessage)
