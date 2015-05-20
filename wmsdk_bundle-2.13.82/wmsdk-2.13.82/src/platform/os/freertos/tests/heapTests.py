import unittest
import pexpect
import time
import time
import cfg

global config
global mtfterm


class TestHeapAlloc(unittest.TestCase):
	if cfg.CONFIG_HEAP_TESTS:
		def checkResult(self,expectedLine):
        		found = 0;
        		line = mtfterm.readline();
        		while 1:
        			#Find two consecutive blank lines (indicating the end)
				if (line == ""):
					line = mtfterm.readline();
					if (line == ""):
						break;
				if (line == expectedLine):
					found = 1;
					break;
				line = mtfterm.readline();
			return found;

		def test(self):
			expectedLine = "\rHEAP-TEST: Success\n"
			mtfterm.sendline("heap-test");
			time.sleep(15);
			failMessage = "FAIL"
			self.failIf(self.checkResult(expectedLine) != 1, failMessage);
