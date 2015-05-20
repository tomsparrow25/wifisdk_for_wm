#!/usr/bin/env python
#
# wlanBurnIn.py: Automated burn-in test for connecting to different networks
#
# Andrey Yurovky <andrey@cozybit.com>
#
# This test performs a "burn-in" by connecting to various types of networks and
# stressing transitions between them.  It requires a network section in the
# config file.  The values in this section contains key-value pairs
# representing a network name and config.  For example:
#
# [wlanBurnIn.networks]
#  myopentest=ssid foo
#  mywpatest=ssid bar wpa2 "big secret"
#
# See Projects/runtests/mtf.sample.cfg for an example.
#
# The burn-in tests will add any networks found, and then connect to each
# network defined there once to sanity-check them.  The tests will then proceed
# to connect to random networks in the list and, finally, remove all the
# networks.
#
# Requirements:
# 1. this test is designed to work with the "runtests" project
# 2. the network config must contain at least one network (preferably
#    several).  We recommend that the config file contains several networks
#    that have varying security and IP address configurations.
# 3. the networks should exist and be usable.

import unittest
import pexpect
import time
import ConfigParser
import random
import sys
from mtfterm import getConfigInt

global config
global mtfterm

class TestWLAN(unittest.TestCase):
	networks = []

	def setUp(self):
		random.seed(time.time())
		try:
			self.networks = config.items('wlanBurnIn.networks')
		except ConfigParser.NoSectionError:
			self.fail("no \"networks\" section in config file")

		mtfterm.sendline('wlan-disconnect')
		self.failIf(mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 10) != True,
			   "Failed to disconnect from current network.");

	def testAddAllNetworks(self):
		# start with a clean network list
		mtfterm.sendline('wlan-list')
		line = mtfterm.readline()
		while line != "0 networks":
			mtfterm.sendline('wlan-list')
			line = mtfterm.readline() # chomp the network count
			net = mtfterm.readline().replace("\"", "")
			mtfterm.sendline("wlan-remove %s" % net)
			line = mtfterm.readline()
			self.failIf(line != "Removed \"%s\"" % net,
						"Failed to remove %s" % net)
			mtfterm.sendline('wlan-list')
			line = mtfterm.readline()

		for network in self.networks:
			if len(network) != 2:
				continue
			mtfterm.sendline("wlan-add %s %s" % network)
			line = mtfterm.readline()
			self.failIf(line != "Added \"%s\"" % network[0],
						"Failed to add network %s: %s" % (network[0], line))

	def testConnectToEachNetwork(self):
		for network in self.networks:
			mtfterm.sendline("wlan-connect \"%s\"" % network[0])
			mtfterm.expect(self, 'Connecting to network...')
			mtfterm.expect(self, "Use 'wlan-stat' for current connection status.")
			self.failIf(not mtfterm.poll_with_timeout("wlan-stat", "Station connected (Active)", 120),
						"Failed to connect to %s" % network[0])
		mtfterm.sendline('wlan-disconnect')
		self.failIf(not mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 10),
					"Failed to disconnect from current network")

	def testConnectToRandomNetworks(self):
		num = len(self.networks)
		iterations = getConfigInt(config, 'wlanBurnIn', 'randomIterations', 20)
		for i in range(iterations):
			n = random.randint(0, num-1)
			mtfterm.sendline("wlan-connect \"%s\"" % self.networks[n][0])
			mtfterm.expect(self, 'Connecting to network...')
			mtfterm.expect(self, "Use 'wlan-stat' for current connection status.")
			self.failIf(not mtfterm.poll_with_timeout("wlan-stat", "Station connected (Active)", 120),
						"Failed to connect to %s" % self.networks[n][0])
		mtfterm.sendline('wlan-disconnect')
		self.failIf(not mtfterm.poll_with_timeout("wlan-stat", "Station disconnected (Active)", 10),
					"Failed to disconnect from current network")

	def testRemoveAllNetworks(self):
		for network in self.networks:
			mtfterm.sendline("wlan-remove %s" % network[0])
			line = mtfterm.readline()
			self.failIf(line != "Removed \"%s\"" % network[0],
						"Failed to remove %s: %s" % (network[0], line))
		mtfterm.sendline('wlan-list')
		self.failIf(mtfterm.readline() != "0 networks")
