import unittest
import pexpect
import time
import re
from mtfterm import getConfigVar
import sys
import urllib2

NETWORKS=r'(\d+) network[s]? found[:]?'
NETWORK_INFO=r'[0-9a-fA-F]{2}\:[0-9a-fA-F]{2}\:[0-9a-fA-F]{2}\:[0-9a-fA-F]{2}\:[0-9a-fA-F]{2}\:[0-9a-fA-F]{2}\s+\S+'
NETWORK_DETAILS=r'\tchannel:\d+\sWMM\:YES|NO\sWEP\:YES|NO\sWPA\:YES|NO\sWPA2\:YES|NO'

re_networks = re.compile(NETWORKS)

global config
global mtfterm

class TestWLAN(unittest.TestCase):

	def tearDown(self):
		# tests may add a network named "netUnderTest" and expect it to be
		# cleaned up by this teardown routine.
		mtfterm.sendline('wlan-remove netUnderTest')
		mtfterm.readline()
		mtfterm.sendline("wlan-ieeeps-off")

	# The connection manager must report one of the supported status results.
	def testStatCommand(self):
		mtfterm.sendline('wlan-stat')
		line = mtfterm.readline()
		self.failIf(line != "Station connected (Active)" and line != "Station connecting (Active)" and
					line != "Station disconnected (Active)",
					"Invalid status: %s" % (line))

	def testAddRemoveCommands(self):
		mtfterm.sendline('wlan-list')
		line = mtfterm.readline()
		while line != "0 networks":
			mtfterm.sendline('wlan-list')
			line = mtfterm.readline() # chomp the network count
			net = mtfterm.readline().replace("\"", "")
			mtfterm.sendline("wlan-remove \"%s\"" % net)
			mtfterm.expect(self, "Removed \"%s\"" % net)
			mtfterm.sendline('wlan-list')
			line = mtfterm.readline()

		# make sure we have 0 networks
		mtfterm.sendline('wlan-list')
		mtfterm.expect(self, '0 networks')
		# add a network, use escaped spaces
		mtfterm.sendline('wlan-add "test network one" ssid tester bssid AA:BB:CC:DD:EE:FF channel 1 ip:192.168.1.100,192.168.1.1,255.255.255.0')
		mtfterm.expect(self, 'Added "test network one"')
		# show the network info
		mtfterm.sendline('wlan-list')
		mtfterm.expect(self, '1 network:')
		mtfterm.expect(self, '"test network one"')
		mtfterm.expect(self, '\tSSID: tester')
		mtfterm.expect(self, '\tBSSID: AA:BB:CC:DD:EE:FF ')
		mtfterm.expect(self, '\tchannel: 1')
		mtfterm.expect(self, '\tmode: sta')
		mtfterm.expect(self, '\tsecurity: none')
		mtfterm.expect(self, '\taddress: STATIC')
		mtfterm.expect(self, '\t\tIP:\t\t192.168.1.100')
		mtfterm.expect(self, '\t\tgateway:\t192.168.1.1')
		mtfterm.expect(self, '\t\tnetmask:\t255.255.255.0')

		# remove the network, use quoted string
		mtfterm.sendline('wlan-remove "test network one"')
		mtfterm.expect(self, 'Removed "test network one"')

		# make sure we have 0 networks
		mtfterm.sendline('wlan-list')
		mtfterm.expect(self, '0 networks')

	def interactiveLinkLost(self):
		mtfterm.sendline('wlan-disconnect')
		self.failIf(mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 30) != True,
					"Failed to initialize into disconnected state");
		devNet = getConfigVar(config, 'devNetwork', 'devnet')
		mtfterm.netConnect(self, 'netUnderTest', devNet)
		print
		print "Poweroff " + devNet + " and press Enter",
		sys.stdin.readline()
		print
		self.failIf(mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 30) != True,
					"Failed to detect link loss");
		mtfterm.getprompt()

	def performanceTestPmkCaching(self):
		mtfterm.sendline('wlan-disconnect')
		self.failIf(mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 30) != True,
					"Failed to initialize into disconnected state");
		wpaNet = getConfigVar(config, 'wpaPskNetwork', 'wpapsknet')
		start = time.time()
		mtfterm.netConnect(self, 'netUnderTest', wpaNet)
		finish = time.time()
		print
		print "time to join wpa network: %.0f seconds" % (finish - start)
		mtfterm.getprompt()

	def testDhcpDns(self):
		mtfterm.sendline('wlan-disconnect')
		self.failIf(mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 30) != True,
					"Failed to initialize into disconnected state");
		internet = getConfigVar(config, 'internet', 'internetDHCP')
		mtfterm.netConnect(self, 'netUnderTest', internet)
		mtfterm.sendline('wlan-gethostbyname marvell.com')
		line = mtfterm.readline()
		self.failIf(re.match("\d*\.\d*\.\d*\.\d*$", line) == None, line)
		mtfterm.getprompt()

	def testStaticDns(self):
		mtfterm.sendline('wlan-disconnect')
		self.failIf(mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 30) != True,
					"Failed to initialize into disconnected state");
		internet = getConfigVar(config, 'internet', 'internetSTATIC')
		mtfterm.netConnect(self, 'netUnderTest', internet)
		mtfterm.sendline('wlan-gethostbyname marvell.com')
		line = mtfterm.readline()
		self.failIf(re.match("\d*\.\d*\.\d*\.\d*$", line) == None, line)
		mtfterm.getprompt()

	def testIEEEPowerSave(self):
		devNet = getConfigVar(config, 'devNetwork', 'devnet')
		mtfterm.netConnect(self, 'devnet', devNet)
		# start the httpd
                mtfterm.sendline("httpd init")
		time.sleep(1)
		mtfterm.expect(self, "httpd init'd")
		mtfterm.sendline("httpd shutdown")
		time.sleep(4)
		mtfterm.sendline("httpd -w init")
		time.sleep(1)
		mtfterm.expect(self, "httpd init'd")
		mtfterm.sendline("httpd -f ftfs init")
		time.sleep(1)
		line = mtfterm.readlines()[3]
		self.failIf("httpd init'd" not in line, "Error: Failed to init httpd")
		mtfterm.sendline("httpd start")
		mtfterm.expect(self, "httpd started")
		ipaddr = mtfterm.get_ipaddr()
		# enter IEEE power save mode
		mtfterm.sendline("pm-wifi-ieeeps-enter")
		mtfterm.readline()
		mtfterm.expect(self, " Enabled IEEE power save mode\n")

		start = time.time()

		# attempt to fetch a page
		msg = ""
		url = ""
		try:
			url = urllib2.urlopen("http://%s/index.html" % (ipaddr))
			if url.getcode() != 200:
				msg = "Expected 200, got %d" % url.getcode()
		except IOError,err:
			msg = str(err)

		# exit IEEE power save mode
		mtfterm.sendline("pm-wifi-ieeeps-exit")
                mtfterm.readline()
                mtfterm.expect(self, " Disabled IEEE power save mode\n")


		self.failIf(msg != "", msg)
		mtfterm.getprompt()
