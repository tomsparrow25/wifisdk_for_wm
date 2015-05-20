import unittest
import time
from mtfterm import getConfigVar
import sys
import socket
from threading import Thread
from curses.ascii import isprint
import cfg

global config
global mtfterm

if cfg.CONFIG_OS_THREADX:
   class TestTTCP(unittest.TestCase):
	def setUp(self):
		devNet = getConfigVar(config, 'devNetwork', 'devnet')
		mtfterm.netConnect(self, 'devnet', devNet)

	def tearDown(self):
		# never leave a ttcp thread running
		mtfterm.sendline("ttcp -k")

	# ttcp-r class for receiving ttcp stream.  Do this in its own thread so we
	# can start it in the background and then start the client.
	class ttcp_r(Thread):
		def __init__(self, length, udp, port, numblocks, ipaddr, verbose=False):
			self.ipaddr = ipaddr
			self.port = port
			self.length = length
			self.udp = udp
			self.numblocks = numblocks
			self.conn = -1
			self.status = -1
			if udp:
				self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
				self.s.bind((self.ipaddr, self.port))
			else:
				self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
				self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
				self.s.bind((self.ipaddr, self.port))
				self.s.listen(1)
			self.s.settimeout(3)
			self.msg = ""
			self.usewheel = verbose
			Thread.__init__(self)

		def run(self):
			index = 0
			wheel = ["\\\b", "|\b", "/\b", "-\b"]
			self.status = -1
			numblocks = self.numblocks
			try:
				if (self.udp):
					data = self.s.recv(4)
					if data != ' !"#':
						self.msg = "unexpected UDP start string"
						return
					self.conn = self.s
				else:
					self.conn, addr = self.s.accept()
			except socket.timeout, e:
				self.msg = "listening socket timed out"
				return

			self.conn.settimeout(3)
			bytes = numblocks*self.length
			if self.usewheel:
				sys.stdout.write(wheel[index])
			expected_char = 0
			offset = 0
			while bytes != 0:
				try:
					data = self.conn.recv(self.length)
				except socket.timeout, e:
					self.msg = "Data socket timed out.  Still expected %d bytes"%bytes
					return

				if len(data) == 4 and data == ' !"#':
					# this is the end of the data stream.  We may not have
					# received all packets, but that's not an error because
					# this is UDP.
					self.status = 0
					return

				for d in data:

					# advance to the next expected char
					while not isprint(expected_char & 0x7F):
						expected_char = (expected_char + 1)  & 0x7F

					if not ord(d) == expected_char:
						self.msg = "Bad data from client.  Expected 0x%x got 0x%x" % \
								   (expected_char, ord(d))
						return
					
					# consume the char and increment the offset
					expected_char = (expected_char + 1)  & 0x7F
					offset = offset + 1
					if offset == self.length:
						expected_char = 0;
						offset = 0;

				bytes = bytes - len(data)
				index = (index + 1) & 0x3
				if self.usewheel:
					sys.stdout.write(wheel[index])
					sys.stdout.flush()

			self.status = 0

		def __del__(self):
			if self.conn != -1:
				self.conn.close()
			self.s.close()

	# ttcp-t class for sending ttcp stream.
	class ttcp_t():
		def __init__(self, length, udp, port, numblocks, ipaddr, verbose=False):
			self.ipaddr = ipaddr
			self.port = port
			self.length = length
			self.udp = udp
			self.numblocks = numblocks
			self.conn = -1
			self.status = -1
			self.address = (self.ipaddr, self.port)
			if udp:
				self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			else:
				self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			self.s.settimeout(3)
			self.msg = ""
			self.usewheel = verbose

		def start(self):
			index = 0
			wheel = ["\\\b", "|\b", "/\b", "-\b"]
			self.status = -1
			try:
				self.s.connect(self.address)
			except socket.timeout, e:
				self.msg = "failed to connect"
				return

			j = 0
			buf = ''
			for i in range(0, self.length):
				while not isprint(j & 0x7F):
					j = j + 1
				buf = buf + chr(j & 0x7F)
				j = j + 1

			if self.usewheel:
				sys.stdout.write(wheel[index])

			if (self.udp):
				self.s.send(' !"#')

			for i in range(0, self.numblocks):
				try:
					self.s.send(buf)
				except socket.timeout, e:
					self.msg = "Data socket timed out sending block %d." % i
					return

				index = (index + 1) & 0x3
				if self.usewheel:
					sys.stdout.write(wheel[index])
					sys.stdout.flush()

			if (self.udp):
				# in practice, the dev host is much faster than the device.
				# This results in UDP packets getting dropped.  We wait for a
				# half second here so that the device can clear some buffer
				# space and actually receive the done signal.
				time.sleep(0.5)
				self.s.send(' !"#')
				self.s.send(' !"#')
				self.s.send(' !"#')
				self.s.send(' !"#')

			self.status = 0
			self.s.close()

	# launch ttcp on the device and do whatever we have to do to make it
	# communicate with us (the host).
	#
	# IMPORTANT: note that we set sockbufsize to 2kB here.  The default for
	# treck is 8kB.  However, this value leads to memory exhaustion, which
	# leads to unexpected timeout errors (260) and OOM errors (255) from treck
	# functions.
	def ttcp(self, client=True, length=1024, udp=False, port=5001,
			 sockbufsize=2*1024, numblocks=2048, verbose=False):

		udparg = ""
		if udp:
			udparg = "-u"

		if client:
			# launch a server and test client functionality
			if udp and length == 4:
				self.fail("ttcp: UDP with len=4 conflicts with start/stop signals")
			ipaddr = getConfigVar(config, 'devNetwork', 'myipaddr')
			server = self.ttcp_r(length, udp, port, numblocks, ipaddr, verbose)
			server.start()
			mtfterm.sendline("ttcp -t -l %d %s -p %d -b %d -n %d %s" %
							 (length, udparg, port, sockbufsize, numblocks, ipaddr))
			if udp:
				mtfterm.expect(self, "ttcp-t: starting udp stream")
			else:
				mtfterm.expect(self, "ttcp-t: connecting to server")
			server.join()
			self.failIf(server.status != 0,	"server failed: " + server.msg)
			server.__del__()

		else:
			# test the server on the device
			ipaddr = mtfterm.get_ipaddr()
			mtfterm.sendline("ttcp -r -l %d %s -p %d -b %d" %
							 (length, udparg, port, sockbufsize))
			mtfterm.expect(self, "ttcp-r: waiting for connection")
			client = self.ttcp_t(length, udp, port, numblocks, ipaddr, verbose)
			client.start()
			self.failIf(client.status != 0,	"client failed: " + client.msg)

		# chomp the two lines of performance info
		mtfterm.readline()
		mtfterm.readline()

	def testSimpleTcpClient(self):
		self.ttcp(length=16, numblocks=1024)

	def testDefaultTcpClient(self):
		self.ttcp()

	def testSimpleUdpClient(self):
		self.ttcp(length=16, udp=True, numblocks=1024)

	def testDefaultUdpClient(self):
		self.ttcp(udp=True)

	def testStartStop(self):
		mtfterm.sendline("ttcp -r")
		mtfterm.expect(self, "ttcp-r: waiting for connection")
		time.sleep(0.5)
		mtfterm.sendline("ttcp -k")
		mtfterm.expect(self, "ttcp-r: killed rx thread")
		mtfterm.sendline("ttcp -r")
		mtfterm.expect(self, "ttcp-r: waiting for connection")
		time.sleep(0.5)
		mtfterm.sendline("ttcp -k")
		mtfterm.expect(self, "ttcp-r: killed rx thread")

	def testSimpleTcpServer(self):
		self.ttcp(client=False, length=16, numblocks=1024)

	def testSimpleUdpServer(self):
		self.ttcp(client=False, length=16, udp=True, numblocks=1024)

	def testDefaultTcpServer(self):
		self.ttcp(client=False)

	def testDefaultUdpServer(self):
		self.ttcp(client=False, udp=True)
