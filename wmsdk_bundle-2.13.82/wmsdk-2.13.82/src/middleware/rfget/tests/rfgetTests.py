import unittest
import time
import ConfigParser
from mtfterm import getConfigVar
import urllib2, urllib, os
import threading
import socket
import SimpleHTTPServer
import SocketServer
import shutil
#import cfg

global config
global mtfterm

# Set the default socket timeout.  None of these requests should take this
# long.
timeout = 10
socket.setdefaulttimeout(timeout)

class TestRFGet(unittest.TestCase):
	FILE_PRINT = "printTest.txt"
	FILE_UP_GOOD = "tmp/runtests_flash.bin"
	FILE_UP_E_CRC = "FW_BadCRC.bin"
	FILE_UP_E_NOFILE = "nofile.bin"
	FILE_UP_E_NOTFW = "FW_NotFW.bin"
	FILE_UP_E_TOSHORT = "FW_Short.bin"

	class TestServer(SocketServer.TCPServer):
		allow_reuse_address = True
		def handle_error(self, request, client_address):
			pass

	class SilentHTTPServerHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
		def log_message(self, format, *args):
			pass

	PORT = 8001
	httpd = TestServer(("", PORT), SilentHTTPServerHandler)

	origDir = os.getcwd()
	testfilesDir = os.path.dirname(__file__) + "/../testfiles/"
	baseAddr = "127.0.0.1"
	baseURL = ""
	
	# Some helper functions

	def startHttpdServer(self):
		httpd_thread = threading.Thread(target=self.httpd.serve_forever)
		httpd_thread.setDaemon(True)
		httpd_thread.start()
		
	def readUntilFirst(self, timeout):
		#if cfg.CONFIG_OS_FREERTOS:
			#Reading warning HTTPC: Warn:  Failed to set SO_SNDBUF: -1
			#and HTTPC: Warn:  Failed to set SO_RCVBUF: -1 in case of FreeRTOS..
		#line = mtfterm.readline()
		#line = mtfterm.readline()

		#Reading the content of the file.
		line = mtfterm.readline()
		while (line == "") and (timeout > 0):
			line = mtfterm.readline()
			timeout = timeout-1
		return line

	def readall(self):
		output = ""
		line = mtfterm.readline()
		while line != "":
			output = output + line
			line = mtfterm.readline()
		return output

	def openFile(self, filename):
		path = self.testfilesDir + filename
		return open(path)

	def readFile(self, filename):
		f = self.openFile(filename)
		flines = f.read()
		f.close
		return flines

	def compareFile(self, remote, filename):
		flines = self.readFile(filename).split("\n")
		flines = [x.strip(" ") for x in flines]
		rlines = remote.replace("\r","").split('\n')
		rlines = [x.strip(" ") for x in rlines]
		x = 0
		for line in flines:
			line = line.replace("\n","").replace("\r","")
			if line != rlines[x]:
				self.fail("Expected %s got %s" % (repr(line), repr(rlines[x])))
			x += 1
		return True

	def cmpUpdateLine(self, line):
		line = line.strip();
		first = line[8:36]
		self.assertEqual(first, 'Updating firmware at address')

	# And now the tests:
	def setUp(self):
		os.chdir(self.testfilesDir)
		self.startHttpdServer()
		devNet = getConfigVar(config, 'devNetwork', 'devnet')
		self.baseAddr = getConfigVar(config, 'devNetwork', 'myipaddr')
		self.baseURL = "http://" + self.baseAddr + ":{0}".format(self.PORT) + "/"
		mtfterm.netConnect(self, 'devnet', devNet)

	def tearDown(self):
		self.httpd.shutdown()
		os.chdir(self.origDir)
	
	def testHelp(self):
		mtfterm.sendline('rfprint')
		mtfterm.expect(self, '[rf] Error: Invalid number of arguments.\n')
	
	def testPrintFile(self):
		mtfterm.sendline('rfprint ' + self.baseURL + self.FILE_PRINT)
		lines = self.readUntilFirst(30)
		self.failIf( lines == "", 'Failed to read lines before timeout.' )
		lines = lines +  self.readall()
		self.assertTrue(self.compareFile(lines, self.FILE_PRINT), 'Failed to properly fetch file: ' + self.baseURL + self.FILE_PRINT)
		
 	def testUpdateBadCRC(self):
		mtfterm.sendline('updatefw ' + self.baseURL + self.FILE_UP_E_CRC)
		line = self.readUntilFirst(30)
		self.cmpUpdateLine(line)
		mtfterm.expect(self, '[rf] Error: File data is not a firmware file\n')
		mtfterm.expect(self, '\r[rf] Error: Firmware update failed\n')

	def testUpdateNoFile(self):
		mtfterm.sendline('updatefw ' + self.baseURL + self.FILE_UP_E_NOFILE)
		line = self.readUntilFirst(30)
		line = line.strip();
		self.assertEqual(line, '[rf] Error: HTTP File Not Found. HTTP status code: 404')
	
 	def testUpdateNotFW(self):
 		mtfterm.sendline('updatefw ' + self.baseURL + self.FILE_UP_E_NOTFW)
		line = self.readUntilFirst(30)
		self.cmpUpdateLine(line)
		mtfterm.expect(self, '[rf] Error: File data is not a firmware file\n')
		mtfterm.expect(self, '\r[rf] Error: Firmware update failed\n')
		

	def testUpdateToShort(self):
		mtfterm.sendline('updatefw ' + self.baseURL + self.FILE_UP_E_TOSHORT)
		line = self.readUntilFirst(30)
		self.cmpUpdateLine(line)
		mtfterm.expect(self, '[rf] Error: Failed to get firmware data for initial validation\n')
		mtfterm.expect(self, '\r[rf] Error: Firmware update failed\n')

	# We've made this test interactive instead of automated.  The reason is
	# that when we run the automated tests we don't want to reboot in the
	# middle.  This dramatically alters the state of the device and may obscure
	# bugs.
	def interactiveTestUpdateGood(self):
		# first copy the current .bin file to 'web server' directory
		try:
			os.mkdir(self.testfilesDir+"tmp")
		except:
			pass
		srcF = self.testfilesDir + "../../../../Projects/runtests/src/runtests_flash.bin"
		dstF = self.testfilesDir + "tmp/runtests_flash.bin"
		shutil.copyfile(srcF, dstF)

		# Send commands
		mtfterm.sendline('updatefw ' + self.baseURL + self.FILE_UP_GOOD)
		line = self.readUntilFirst(60)
		partition = line[31:32]
		self.cmpUpdateLine(line)
		line = self.readUntilFirst(60)
		self.assertEqual(line, 'Firmware update succeeded')

		# test reboot
		mtfterm.sendline('pm-reboot')
		line = self.readUntilFirst(10)
		line = mtfterm.readline()
		first = line[0:23]
		bootpart = line[23:24]
		last = line[24:]
		self.assertEqual(first, ' - Firmware Partition: ')
		self.assertEqual(partition, bootpart)
		self.assertEqual(last, ' (main firmware)')

		# Cleanup
		shutil.rmtree(self.testfilesDir+"tmp")

