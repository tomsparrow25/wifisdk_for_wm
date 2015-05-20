import unittest
import time
import ConfigParser
from mtfterm import getConfigVar
import urllib2, urllib, os
from threading import Thread
import socket

global config
global mtfterm

# Set the default socket timeout.  None of these requests should take this
# long.
timeout = 10
socket.setdefaulttimeout(timeout)

class TestHTTPD(unittest.TestCase):

	# Some helper functions
	def openPage(self, pagename, ipaddr=None):
		if ipaddr == None:
			ipaddr = mtfterm.get_ipaddr()
		try:
			url = urllib2.urlopen("http://%s/%s" % (ipaddr, pagename))
		except urllib2.HTTPError,err:
			return err
		except IOError,err:
			self.fail("Failed to contact server: %s" % str(err))
		return url

	def expect(self, line, expected):
		self.failIf(line != expected,
					"Expected %s got %s" % (repr(expected), repr(line)))

	def readall(self, url):
		output = ""
		line = url.readline()
		while line != "":
			output = output + line
			line = url.readline()
		return output

	def openFile(self, filename):
		path = os.path.dirname(__file__) + "/../testfiles/" + filename
		return open(path)

	def readFile(self, filename):
		f = self.openFile(filename)
		flines = f.read()
		f.close
		return flines

	def compareFile(self, remote, filename):
		flines = self.readFile(filename).split("\n")
		rlines = remote.replace("\r","").split("\n")
		x = 0
		for line in flines:
			line = line.replace("\n","").replace("\r","")
			if line != rlines[x]:
				self.fail("Expected %s got %s" % (repr(line), repr(rlines[x])))
			x += 1
		return True

	def postPage(self, pagename, postvars, ipaddr=None):
		if ipaddr == None:
			ipaddr = mtfterm.get_ipaddr()
		try:
			#data = urllib.urlencode(postvars)g
			#req = urllib2.Request("http://%s/%s" % (ipaddr, pagename), data)
			#url = urllib2.urlopen(req)
			url = urllib2.urlopen("http://%s/%s" % (ipaddr, pagename),
								 urllib.urlencode(postvars))
		except urllib2.HTTPError,err:
			return err
		except IOError,err:
			self.fail("Failed to contact server: %s" % str(err))
		return url

	def startHttpd(self, options=""):
		mtfterm.sendline("httpd " + options + " init")
		if (options == "-w" or options == ""):
			mtfterm.expect(self, "httpd init'd")
		else:
			line = mtfterm.readlines()[3]
			self.failIf("httpd init'd" not in line, "Error: Failed to init httpd")
		mtfterm.sendline("httpd start")
		mtfterm.expect(self, "httpd started")
		#print "Started httpd"

	# helper class that gets a page in its own thread
	class getThreaded(Thread):
		def __init__(self, page, testcase, ipaddr):
			self.page = page
			self.testcase = testcase
			self.ipaddr = ipaddr
			Thread.__init__(self)
			self.status = -1

		def run(self):
			url = self.testcase.openPage(self.page, self.ipaddr)
			self.status = url.getcode()
			url.close()
			return

	# And now the tests:
	def setUp(self):
		# Always start with httpd disabled and with a connection to devNet
		mtfterm.sendline("httpd init")
		mtfterm.sendline("httpd shutdown")
		time.sleep(4)
		mtfterm.getprompt()
		devNet = getConfigVar(config, 'devNetwork', 'devnet')
		mtfterm.netConnect(self, 'httpddevnet', devNet)
		#print "Done setup"

	def testInitShutdown(self):
		mtfterm.sendline("httpd init")
		mtfterm.expect(self, "httpd init'd")
		mtfterm.sendline("httpd shutdown")
		time.sleep(4)
		mtfterm.expect(self, "httpd shutdown")
		mtfterm.sendline("httpd init")
		mtfterm.expect(self, "httpd init'd")
		mtfterm.sendline("httpd shutdown")
		time.sleep(4)
		mtfterm.expect(self, "httpd shutdown")

	def testStartStop(self):
		mtfterm.sendline("httpd init")
		mtfterm.expect(self, "httpd init'd")
		mtfterm.sendline("httpd start")
		mtfterm.expect(self, "httpd started")
		mtfterm.sendline("httpd stop")
		time.sleep(4)
		mtfterm.expect(self, "httpd stopped")
		mtfterm.sendline("httpd start")
		mtfterm.expect(self, "httpd started")
		mtfterm.sendline("httpd stop")
		time.sleep(4)
		mtfterm.expect(self, "httpd stopped")
		mtfterm.sendline("httpd shutdown")
		time.sleep(4)
		mtfterm.expect(self, "httpd shutdown")

	def testNoSuchWSGIHandler(self):
		# Initialize with no handlers and no fs
		self.startHttpd()
		url = self.openPage("nosuchwsghandler")
		self.failIf(url.getcode() != 404,
					"Expected 404, got %d" % url.getcode())

	def testSimpleGet(self):
		self.startHttpd("-f ftfs -w")
		url = self.openPage("testget.txt")
		self.failIf(url.getcode() != 200,
					"Expected 200, got %d" % url.getcode())
		self.compareFile(self.readall(url), "testget.txt")

	def testMaxUriLength(self):
		# In the default configuration, URLs can't be any longer than
		# 64 bytes (HTTPD_MAX_URI_LENGTH).
		self.startHttpd("-f ftfs -w")
		url = self.openPage("index.html?this=uri&is=way&too=longmakeitlargerthanmaximumcharactersallowed")
		self.failIf(url.getcode() != 500,
					"Expected Server error 500, got %d" % url.getcode())
		self.expect(self.readall(url),
					"Error processing token: filename. Filename missing or too long?");

	def testSimplePost(self):
		self.startHttpd("-w")
		postvars = {"foo":"bar", "this":"that"}
		url = self.postPage("postecho", postvars)
		self.failIf(url.getcode() != 200,
					"Expected 200 Success, got %d" % url.getcode())
		echoed = url.readline();
		response = {}
		while echoed != "":
			echoed = echoed.replace("\r\n","").split("=");
			response[echoed[0]] = echoed[1]
			echoed = url.readline();
		self.expect(response["this"], "that");
		self.expect(response["foo"], "bar");
		url.close()

	def testFileShtml(self):
		self.startHttpd("-f ftfs -w")
		url = self.openPage("testshtml.shtml");
		self.failIf(url.getcode() != 200,
					"Expected 200, got %d" % url.getcode())
		lines = self.openFile("header.txt").readlines()
		lines = lines + self.openFile("body.txt").readlines()
		lines = lines + self.openFile("footer.txt").readlines()
		#print "lines: %s. url: %s." %(lines, url.readlines())
		for line in lines:
			self.expect(url.readline(), line)
		self.expect(url.readline(), "")

	def testSimpleVirtualShtml(self):
		self.startHttpd("-f ftfs -w")
		url = self.openPage("testvirtual.shtml");
		self.failIf(url.getcode() != 200,
					"Expected 200, got %d" % url.getcode())
		self.expect(url.readline(), "foobar\r\n")
		self.expect(url.readline(), "foobar\r\n")
		self.expect(url.readline(), "foobar\r\n")
		self.expect(url.readline(), "foobar\r\n")
		self.expect(url.readline(), "")

	def testShtmlArgs(self):
		self.startHttpd("-f ftfs -w")
		url = self.openPage("testecho.shtml");
		self.failIf(url.getcode() != 200,
					"Expected 200, got %d" % url.getcode())
		# empty arg list and leading space list should return nothing.  So line
		# 3 of testecho.shtml is the first one that we expect to result in any
		# output.
		self.expect(url.readline(), "1 2 3 4 5\r\n")
		# quotes are allowed
		self.expect(url.readline(), "1 2 3 4 5\r\n")
		# leading space is allowed
		self.expect(url.readline(), "1 2 3 4 5\r\n")
		# double quotes are reserved
		self.expect(url.readline(), "1 2 3 \r\n")
		# maximum length arg list should work fine
		self.expect(url.readline(), "list exactly HTTPD_MAX_SSI_ARGS \r\n")
		# leading space is ignored
		self.expect(url.readline(), "leading space doesn't count\r\n")
		self.expect(url.readline(), "trailing space does count       \r\n")
		# arg list exceeding HTTPD_MAX_SSI_ARGS (32 by default) should truncate
		self.expect(url.readline(), "this list should be truncated be\r\n")
		# empty function should not return anything
		self.expect(url.readline(), "")

	# def testUrlEncodingReserved(self):
	# 	self.startHttpd("-w")
	# 	postvars = {"allreserved":"!*'();:@&+$,/?%#[]",
	# 				"!*'();:@&+$,/?%#[]":"allreserved",
	# 				"this tag has spaces":"this val has spaces"}
	# 	url = self.postPage("postecho", postvars)
	# 	self.failIf(url.getcode() != 200,
	# 				"Expected 200 Success, got %d" % url.getcode())
	# 	echoed = url.readline();
	# 	response = {}
	# 	while echoed != "":
	# 		echoed = echoed.replace("\r\n","").split("=");
	# 		response[echoed[0]] = echoed[1]
	# 		echoed = url.readline();
	# 	self.expect(response["allreserved"], "!*'();:@&+$,/?%#[]")
	# 	self.expect(response["!*'();:@&+$,/?%#[]"], "allreserved")
	# 	self.expect(response["this tag has spaces"], "this val has spaces")
	# 	postvars = {"contains equals":"foo=bar"}

	# def testUrlEncodingEquals(self):
	# 	self.startHttpd("-w")
	# 	postvars = {"contains equals":"foo=bar"}
	# 	url = self.postPage("postecho", postvars)
	# 	self.failIf(url.getcode() != 200,
	# 				"Expected 200 Success, got %d" % url.getcode())
	# 	self.expect(url.readline(), "contains equals=foo=bar\r\n")
	# 	self.expect(url.readline(), "")

	# def testShtmlPost(self):
	# 	self.startHttpd("-f ftfs -w")
	# 	postvars = {"foo":"bar","good":"evil"}
	# 	url = self.postPage("testpost.shtml", postvars);
	# 	self.failIf(url.getcode() != 200,
	# 				"Expected 200, got %d" % url.getcode())
	# 	#print "response: %s" %url.read()
	# 	self.expect(url.readline(), "<html>\n")
	# 	self.expect(url.readline(), "<ul>\n")
	# 	self.expect(url.readline(), "<li>good=evil</li>\r\n")
	# 	self.expect(url.readline(), "<li>foo=bar</li>\r\n")
	# 	self.expect(url.readline(), "</ul>\n")
	# 	self.expect(url.readline(), "</html>\n")
	# 	self.expect(url.readline(), "")

	# def testShtmlPostWithTemplate(self):
	# 	self.startHttpd("-f ftfs -w")
	# 	postvars = {"foo":"bar","good":"evil",
	# 				"template":"<li>value is %v tag is %t</li>\r\n"}
	# 	url = self.postPage("testpost.shtml", postvars);
	# 	self.failIf(url.getcode() != 200,
	# 				"Expected 200, got %d" % url.getcode())
	# 	self.expect(url.readline(), "<html>\n")
	# 	self.expect(url.readline(), "<ul>\n")
	# 	self.expect(url.readline(), "<li>value is evil tag is good</li>\r\n")
	# 	self.expect(url.readline(), "<li>value is bar tag is foo</li>\r\n")
	# 	self.expect(url.readline(), "</ul>\n")
	# 	self.expect(url.readline(), "</html>\n")
	# 	self.expect(url.readline(), "")

	def testManyChunks(self):
		size = 256
		num = 256
		self.startHttpd("-f ftfs -w")
		postvars = {"size":str(size),"num":str(num)}
		url = self.postPage("memdump", postvars);
		self.failIf(url.getcode() != 200,
					"Expected 200, got %d" % url.getcode())
		for i in range(num):
			self.expect(url.readline(), "x"*size + "\r\n")
		self.expect(url.readline(), "")

	def testMaxBacklog(self):
		# we should be able to have at least HTTPD_MAX_BACKLOG_CONN (5 in the
		# default config) connections at a time.
		self.startHttpd("-f ftfs -w")
		time.sleep(120)
		maxconn = 5
		threads = {}
		ipaddr = mtfterm.get_ipaddr()
		for i in range(maxconn):
			threads[i] = self.getThreaded("index.html", self, ipaddr)
			threads[i].start()

		for i in range(maxconn):
			threads[i].join()
			self.failIf(threads[i].status != 200, "Failed to get instance %d" % i)

	def testHttpV10(self):
		# we don't support http v1.0
		self.startHttpd("-f ftfs -w")
		ipaddr = mtfterm.get_ipaddr()
		try:
			url = urllib.urlopen("http://%s/index.html" % (ipaddr))
			self.failIf(url.getcode() != 505, "HTTP V1.0 GET should return 505")
		except IOError,err:
			self.fail("Failed to contact server: %s" % str(err))

	def testFTFSGet(self):
		# Note that this test is not exhaustive.  It will pass as long as there
		# is a file called index.html in the ftfs.
		self.startHttpd("-f ftfs -w")
		url = self.openPage("index.html")
		self.failIf(url.getcode() != 200,
					"Expected 200, got %d" % url.getcode())

	def testUserAgent(self):
		path = os.path.dirname(__file__) + "/useragentstrings.txt"
		f = open(path)
		flines = f.readlines()
		f.close
		for l in flines:
			l = l.replace("\n", "")
			parts = l.split("\t")
			mtfterm.sendline('httpd-useragent "' + parts[2] + '"')
			mtfterm.expect(self, parts[0] + " " + parts[1])
