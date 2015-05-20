import unittest
import pexpect
import time

global config
global mtfterm


class TestPSM(unittest.TestCase):
		
	def setUp(self):
		mtfterm.sendline("psm-erase");
		time.sleep(5);
		mtfterm.sendline("psm-register test-mod testprt");
		mtfterm.getprompt()

 	def testPsmSetGet(self):
		# TODO: psm-get should read from flash here. Add a psm-invalidate-cache 
		# command to make sure this happens
		mtfterm.sendline("psm-set test-mod var val");
 		mtfterm.sendline("psm-get test-mod var");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] Value: val\r\n", "Expected '[psm] Value: val\r\n' got '%s'" % line)

	def testPsmNonExistantModule(self):
		mtfterm.sendline("psm-set no-mod var val");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] Error: psm_open failed with: -23 (Is the module name registered?)\n", "Expected '[psm] Error: psm_open failed with: -23 (Is the module name registered?)\n' got '%s'" % line)
		mtfterm.sendline("psm-get no-mod var");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] Error: psm_open failed with: -23 (Is the module name registered?)\n", "Expected '[psm] Error: psm_open failed with: -23 (Is the module name registered?)\n' got '%s'" % line)

	def testPsmNonExistantVariable(self):
		mtfterm.sendline("psm-get test-mod novar");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] Value: \r\n", "Expected '[psm] Value: val\r\n' got '%s'" % line)


	def testPsmPostEraseSanity(self):
 		mtfterm.sendline("psm-erase");
 		time.sleep(10);

 		mtfterm.sendline("psm-dump 0");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] last_used=0\r\n", "Expected '[psm] last_used=0\r\n' got '%s'" % line)

 		mtfterm.sendline("psm-set test-mod var val");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] Error: psm_open failed with: -23 (Is the module name registered?)\n", "Expected '[psm] Error: psm_open failed with:-23 (Is the module name registered?)\n' got '%s'" % line)
		mtfterm.sendline("psm-register test-mod testprt");
 		mtfterm.sendline("psm-set test-mod var val");
 		mtfterm.sendline("psm-get test-mod var");
		line=mtfterm.readlines()[0]
		self.failIf(line != "[psm] Value: val\r\n", "Expected '[psm] Value: val\r\n' got '%s'" % line)

 
	def testPsmStressTest(self):
 		for i in range(1, 16):
 			cmd = "psm-set test-mod " + str(i) + " " + str(i)
			mtfterm.sendline(cmd)
 		for i in range(1, 16):
 			cmd = "psm-get test-mod " + str(i)
 			mtfterm.sendline(cmd)
			line=mtfterm.readlines()[0]
		       	self.failIf(line!= "[psm] Value: " + str(i) +"\r\n","Expectd '[psm] Value: "+ str(i) +"\r\n' got '%s'"% line)
			#mtfterm.expect(self, "[psm] Value: " + str(i) +"\r\n")
			#mtfterm.expect(self,line)

