#!/usr/bin/python

import serial, thread, threading, sys, signal, getopt, termios, os
import ConfigParser, unittest
from time import sleep

if os.path.basename(os.getcwd()) != "WMSDK":
	print "You need to run this from the WMSDK directory\n"
	sys.exit(1)

os.system("tools/bin/test/generatecfg .config tools/bin/cfg.py");

# You probably never want to set this to true.  It makes a mess of your output.
# It's only useful if there's a problem with mtfterm.
DEBUG = False

# Generally, you'll want your tests to be pretty quiet.  But sometimes,
# especially when a test is failing and you don't know why, you may want to
# view the logging output.  Normally, this would totally confuse the tests,
# because they depend on the serial stream for test results.  Logging is noise.
# Unfortunately, we don't have a different channel for logging.  Here's how to
# add logging for your module:
#
# 1. Compile your module with debug logging and ensure that every debug line
# starts with a unique prefix.
#
# 2. Add the unique prefix to the LOGGING_PREFIXES array below
#
# 3. Set LOGGING to True and run your tests.  Expect the logging output to be
# printed along with the test output.
#
# Caveat 1: your unique prefix must not match any real test output.  For
# example, if you chose your prefix to be "test" and some other test depends on
# the words "test completed" showing up on the console, you'd swallow that
# test's expected output.
#
# Caveat 2: if logging is enabled in your build, you MUST enable it here.
#
# Caveat 3: Test output will be interleaved with logging output.  This may be
# confusing.
LOGGING = False
#AUTOTEST must be used when we want to run the tests with the cron tasks which 
#will not have a controlling terminal 
AUTOTEST = False
LOGGING_PREFIXES = ["HTTPD:", "wlan:", "Runtests:", "TCP:"]

class mtfterm():

	def __init__(self, port, quiet=False):
		self.mutex = thread.allocate_lock()
		self.serial = serial.Serial(port, 115200, timeout=1)
		self.serial.flushInput()
		self.serial.flushOutput()
		self.quiet = quiet
		self.LOGGING = LOGGING

                if AUTOTEST:
                        self.serial.write('\r')

			if quiet:
				fd = sys.stdin.fileno()
				old = termios.tcgetattr(fd)
				new = termios.tcgetattr(fd)
				new[3] = new[3] & ~termios.ECHO
				termios.tcsetattr(fd, termios.TCSADRAIN, new)
				self.serial.write('\r')

	def __del__(self):
		if self.LOGGING:
			# push any remaining output
			line = self.serial.readline()
			while line != "":
				print line,
				line = self.serial.readline()
                if AUTOTEST:
                        if self.quiet:
                                sys = __import__("sys")
                                termios = __import__("termios")
		else:
			if self.quiet:
				# for some reason, when our destructor is called sys and termios
				# are no longer available.  So we reimport them.
				sys = __import__("sys")
				termios = __import__("termios")
				fd = sys.stdin.fileno()
				old = termios.tcgetattr(fd)
				new = termios.tcgetattr(fd)
				new[3] = new[3] | termios.ECHO
				termios.tcsetattr(fd, termios.TCSADRAIN, new)

		self.serial.flushInput()
		self.serial.flushOutput()
		self.serial.close()

	def islogline(self, line):
		for p in LOGGING_PREFIXES:
			if line.startswith(p):
				return True
		return False

	def terminal(self):
		while 1:
			self.getprompt()
			sys.stdout.write("# ")
			line = sys.stdin.readline()
			line = line.replace("\n", "")
			line = line.replace("\r", "")
			self.sendline(line)

	def readline(self):
		line = self.serial.readline()
		while self.islogline(line):
			if LOGGING:
				print line,
			line = self.serial.readline()

		line = line.replace("\r\n", "")
		line = line.replace("#", "")
		if LOGGING:
			# if we're logging, echo the line
			print line
		if DEBUG:
			print "Read: ",line
		return line

	def sendline(self, line):
		# If we're not in quite mode, ensure that we are at a prompt.
		# Otherwise, the reader thread will handle that.
		if self.quiet:
			self.getprompt()

		if DEBUG:
			print "Writing: ",line

		self.serial.write("%s\r"%(line))
		# chomp the echo'd line so it doesn't appear in the output stream.
		# ASSUME that we will always get the echo'd line, and that it will
		# always be the next line we see.  Therefore if we see a blank line,
		# it's because we timed out waiting for the prompt.  Note that it is
		# also entirely possible that the device is just taking a really long
		# time to respond (e.g., the command is blocking unexpectedly.)  Here
		# we give it 10 seconds to come back.  This is an arbitrary large
		# number.
		line = ""
		tries = 0
		while line == "" and tries < 10:
			line = self.serial.readline()
			tries += 1
		if DEBUG:
			print "Chomped: ",line

		if LOGGING:
			# if we're logging, echo the sent line
			print "# " + line

	def getprompt(self):
		data = self.serial.read()
		while (data != "#" and data != ""):
			if not self.quiet:
				sys.stdout.write(data)
			elif LOGGING:
				# print log lines
				print data + self.serial.readline(),
			data = self.serial.read()

		# if we timed out reading (i.e., data == ""), assume we're at the
		# prompt.  Perhaps we could keep some state around and optimize away
		# the timeout.
		if data == "#":
			# chomp the space after the prompt
			data = self.serial.read()

		if DEBUG and data == "":
			print "Timed out waiting for prompt"

	def poll_with_timeout(self, status_command, expected_status, timeout):
		i = 0
		while 1:
			self.sendline(status_command)
			line = self.readline()
			if line == expected_status:
				return True
			sleep(2)
			i += 1
			if i >= timeout:
				return False

	def readlines(self):
            lines = self.serial.readlines()
	    #print lines
            return lines

	def expect(self, testcase, expectedLine,failMessage=None):
		line = self.readline();
		if failMessage == None:
			failMessage = "Expected '%s' got '%s'" % (expectedLine, line)
		testcase.failIf(expectedLine != line, failMessage)

	def get_ipaddr(self):
		self.sendline("wlan-address")
		line = self.readline()
		if (line == "not connected"):

			return "0.0.0.0"
		else:
			line = self.readline()
			return line.split("\t")[4]

	# NOTE: if the network called netname exists, it will be joined.
	# Otherwise, it will be added with the supplied netcfg and then joined.
	# So, be sure to remove netname after using it, otherwise other tests that
	# use the same netname may have unexpected results.
	def netConnect(self, testcase, netname, netcfg):
		testcase.failIf(netcfg == None, "net configuration required");
		self.sendline('wlan-info')
		line = self.readline()
		if line == "Station connected to:":
			line = self.readline()
			if line == "\"" + netname + "\"":
				return
		self.sendline("wlan-add " + netname + " " + netcfg)
		line = self.readline()
		testcase.failIf(line != 'Added "' + netname + '"' and
						line != 'Error: that network already exists',
						"Failed to add network " + netname + ": " + line)
		self.sendline("wlan-connect " + netname)
		self.expect(testcase, 'Connecting to network...')
		self.expect(testcase, "Use 'wlan-stat' for current connection status.")
		testcase.failIf(not self.poll_with_timeout("wlan-stat", "Station connected (Active)", 120),
						"Failed to connect to " + netname)
		print "Connected to %s successfully" %netname

	def isAlive(self):
		self.sendline("help")
		line1 = self.readline()
		line2 = self.readline()
		self.serial.flushInput()
		self.serial.flushOutput()
		self.getprompt()
		if 'help' in line1 or 'help' in line2:
			return True
		return False

def getConfigVar(config, section, var, default=None):
	try:
		val = config.get(section, var)
	except ConfigParser.NoSectionError, err:
		val = default
	return val

def getConfigInt(config, section, var, default=None):
	try:
		val = config.getint(section, var)
	except ConfigParser.NoSectionError, err:
		val = default
	return val

def usage():
	print """
mtfterm: A terminal for the Marvell WISE module tests
  -h               Print this message

  -a               This option should be used only when you want to
                   run the tests by scheduling it as a cron task

  -c <cfg>         Config file for the tests.  If this is not provided mtfterm
                   will use ./mtf.cfg.  If that file does not exist, mtfterm
				   attempts to open /dev/ttyUSB0 and proceed according to the
				   other options.

  -t <test.py[:testname]>
                   If this option is provided, mtfterm attempts to run the unit
                   tests in test.py.  If testname is specified, only that test
				   within test.py will be run.  If no -t option is provided,
				   mtfterm simply opens a terminal.
"""

if __name__ == '__main__':

	try:
		opts, args = getopt.getopt(sys.argv[1:], "hac:t:l",
								   ["help", "autotest", "configfile=", "testfile=", "logging"])
	except getopt.GetoptError, err:
		print str(err)
		sys.exit(2)

	configfile = None
	testfile = None
	singletest = None
	for o, a in opts:
		if o in ("-h", "--help"):
			usage()
			sys.exit()
                if o in ("-a", "--autotest"):
                        AUTOTEST=True
		elif o in ("-c", "--configfile"):
			configfile = a
		elif o in ("-t", "--testfile"):
			testfile = a
			parts = testfile.split(":")
			if len(parts) == 2:
				singletest = parts[1]
				testfile = parts[0]
		elif o in ("-l", "--logging"):
			LOGGING=True
		else:
			assert False, "unhandled option"

	# set up the configuration
	if configfile == None and os.path.exists("./mtf.cfg"):
		configfile = "./mtf.cfg"
	if configfile != None and not os.path.exists(configfile):
		print "Config file ",configfile," does not exist."
		sys.exit(4)

	config = ConfigParser.ConfigParser()
	if configfile != None:
		config.read(configfile)
	port = getConfigVar(config, 'serialport', 'port', '/dev/ttyUSB0')

	if testfile == None:
		# Run in interactive mode
		try:
			mtfterm = mtfterm(port, False)
		except serial.serialutil.SerialException, err:
			print str(err)
			sys.exit(2)
		try:
			mtfterm.terminal()
		except KeyboardInterrupt:
			print ""
			sys.exit(0)

	else:
		if not os.path.exists(testfile):
			print "Test file ",testfile," does not exist."
			sys.exit(3)
		# load the module
		modulename = os.path.splitext(os.path.basename(testfile))[0]
		module = __import__(modulename)
		module.config = config

		# set up the mtfterm
		mtfterm = mtfterm(port, quiet=True)
		module.mtfterm = mtfterm

		# test communication to device
		if not mtfterm.isAlive():
			print "Failed to communicate with device!"
			sys.exit(4)

		# run the tests
		try:
			if (singletest != None):
				suite = unittest.TestLoader().loadTestsFromName(singletest, module)
			else:
				suite = unittest.TestLoader().loadTestsFromModule(module)
			unittest.TextTestRunner(verbosity=2).run(suite)
		except KeyboardInterrupt:
			sys.exit(0)
		except AttributeError:
			print "test file does not contain a test called ",singletest
