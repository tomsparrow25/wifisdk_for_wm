import unittest
import pexpect
import time

global config
global mtfterm

class TestJSON(unittest.TestCase):
		
	def testjsonparsing(self):
		mtfterm.sendline("json-test");
		mtfterm.expect(self, "Success");

