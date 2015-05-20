import unittest
import pexpect
import time
#import cfg

global config
global mtfterm

class TestDHCP(unittest.TestCase):
	def setUp(self):
		mtfterm.sendline('uaputl sys_cfg_ssid myuap');
		mtfterm.sendline('uaputl sys_cfg_wpa_passphrase marvell89');
		mtfterm.sendline('uaputl bss_start');
		time.sleep(7);
		mtfterm.sendline('wlan-stat');
		time.sleep(1)
		lines = mtfterm.readlines()
		#print "Read line: %s" %lines
		self.failIf("uAP started (Active)\r\n" not in lines, "Failed to start uap");
		mtfterm.sendline('wlan-start-network uap');
		mtfterm.sendline('\n');
		#print "Setup successful"

	def tearDown(self):
		mtfterm.sendline('wlan-disconnect uap');
		mtfterm.sendline('wlan-remove uap');

	def checkDhcpStatus(self):
		found = 0;
		expectedLine = "dhcp-server:";
		mtfterm.sendline("sysinfo thread");
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

	def testDhcpSrvMlanStart(self):
		error = 0;
		if (self.checkDhcpStatus() == 1):
			error = error + 1;
			failMessage = "dhcp server is already running"
			self.failIf(error != 0, failMessage);
		else:
			mtfterm.sendline('dhcp-server mlan0 start');
			time.sleep(3);
			failMessage = "Error in starting dhcp server";
			self.failIf(self.checkDhcpStatus() != 1, failMessage);

	def testDhcpSrvMlanStop(self):
		error = 0;
		if (self.checkDhcpStatus() == 0):
			error = error + 1;
			failMessage = "dhcp server is not running"
			self.failIf(error != 0, failMessage);
		else:
			mtfterm.sendline('dhcp-server mlan0 stop');
			time.sleep(3);
			failMessage = "Error in stopping dhcp server";
			self.failIf(self.checkDhcpStatus() != 0, failMessage);

	def testDhcpSrvUapStart(self):
		error = 0;
		if (self.checkDhcpStatus() == 1):
			error = error + 1;
			failMessage = "dhcp server is already running"
			self.failIf(error != 0, failMessage);
		else:
			mtfterm.sendline('dhcp-server uap0 start');
			time.sleep(3);
			failMessage = "Error in starting dhcp server";
			self.failIf(self.checkDhcpStatus() != 1, failMessage);

	def testDhcpSrvUapStop(self):
		error = 0;
		if (self.checkDhcpStatus() == 0):
			error = error + 1;
			failMessage = "dhcp server is not running"
			self.failIf(error != 0, failMessage);
		else:
			mtfterm.sendline('dhcp-server uap0 stop');
			time.sleep(3);
			failMessage = "Error in stopping dhcp server";
			self.failIf(self.checkDhcpStatus() != 0, failMessage);

	def testDhcpSrvHelp(self):
		time.sleep(1);
		mtfterm.sendline('\n');
		mtfterm.sendline('dhcp-server');
		expectedLine = "Usage: dhcp-server <mlan0/uap0/wfd0> <start|stop>Error: invalid argument";
		failMessage = "Inappropriate dhcp-server help";
		error = 0;
		line = mtfterm.readline();
		if (line != expectedLine):
			error = error + 1;
			print("Failed for input 'dhcp-server'. Expected: " + str(expectedLine) + " and Found: " + str(line))

		mtfterm.sendline('dhcp-server test');
		line = mtfterm.readline();
		if (line != expectedLine):
			error = error + 1;
			print("Failed for input 'dhcp-server test'. Expected: " + str(expectedLine) + " and Found: " + str(line))
		mtfterm.sendline('dhcp-server test incorrect_param');
		line = mtfterm.readline();
		if (line != expectedLine):
			error = error + 1;
			print("Failed for input 'dhcp-server test incorrect_param'. Expected: " + str(expectedLine) + " and Found: " + str(line))
		self.failIf(error != 0, failMessage);

