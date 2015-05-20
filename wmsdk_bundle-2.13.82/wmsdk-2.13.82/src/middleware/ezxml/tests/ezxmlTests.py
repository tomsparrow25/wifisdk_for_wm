import unittest
import pexpect
import time

global config
global mtfterm

class TestEzxml(unittest.TestCase):

	def testparsing_test(self):
		mtfterm.sendline("xml-parsing-test");
		mtfterm.expect(self, "Success");

	def testcreation_test1(self):
		mtfterm.sendline("xml-creation-test1");
		mtfterm.expect(self, "Success");

	def testcreation_test2(self):
		mtfterm.sendline("xml-creation-test2");
		mtfterm.expect(self, "Success");
