import unittest

global config
global mtfterm

class TestSysinfo(unittest.TestCase):

	def testSingleThreadInfo(self):
		mtfterm.sendline("sysinfo -n cli thread")
		mtfterm.expect(self, "cli:")

	def testNoSuchThread(self):
		mtfterm.sendline("sysinfo -n foobar thread")
		mtfterm.expect(self, "ERROR: No thread named 'foobar'")
