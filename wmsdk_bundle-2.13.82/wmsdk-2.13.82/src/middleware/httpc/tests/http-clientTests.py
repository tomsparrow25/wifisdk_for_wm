import unittest
import pexpect
import time
from mtfterm import getConfigVar
import commands
global config
global mtfterm

class TestHTTPCLIENT(unittest.TestCase):

    def setUp(self):
        mtfterm.sendline("Module http-client registered");
        mtfterm.getprompt()
        devNet = getConfigVar(config, 'devNetwork', 'devnet')
        self.baseAddr = getConfigVar(config, 'devNetwork', 'myipaddr')
        self.baseURL = "\"http://" + self.baseAddr
        mtfterm.netConnect(self, 'devnet', devNet)

    def checkResult(self,expectedLine):
	found = 0;
        line = mtfterm.readline();
        #print "line: %s" %line
        while 1:
        #Find two consecutive blank lines (indicating the end)
		if (line == ""):
			line = mtfterm.readline()
			if (line == ""):
                    		break;
		line = line.strip();
		if (line == expectedLine):
                    found = 1;
                    break;
		line = mtfterm.readline();
	return found;

    #This will connect the http server and check that it can able to connect or not.
    def testSimpleGet(self):
	command =  "rfprint " + self.baseURL + "/index.html\"";
        mtfterm.sendline(command);
	time.sleep(1);
	expectedLine = '<html><body><h1>It works!</h1></body></html>'
	failMessage = 'testSimpleGet Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);


    #This test case will try to request page from server which is not present on
    #the server.
    def testPageNotFound(self):
	command =  "rfprint " + self.baseURL + "/hello.txt\"";
        mtfterm.sendline(command);
	time.sleep(1);
	expectedLine = '[rf] Error: HTTP File Not Found. HTTP status code: 404'
	failMessage = 'testPageNotFound Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This is test will try to connect the host where http server is not running.
    def testUnableToConnect(self):
	ip = "10.31.131.180"
	command = "rfprint" + " \"http://" + ip + "\"";
        mtfterm.sendline(command);
	time.sleep(90);
	expectedLine = '[httpc] Error: tcp connect failed 10.31.131.180:80'
	failMessage = 'testUnableToConnect Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This test case will try to run different format url.
    def testUrlValidation(self):
	command = "rfprint \"1-1.168.1.12\""
        mtfterm.sendline(command);
	time.sleep(50);
	expectedLine = '[httpc] Error: tcp connect failed 1-1.168.1.12:80'
	failMessage = 'testUrlValidation Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);
        command = "rfprint \"https://1-1.168.1.12\""
        mtfterm.sendline(command);
	time.sleep(50);
	expectedLine = '[rf] Error: HTTPS URLs are currently not supported for rfprint'
	failMessage = 'testUrlValidation Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);
        command = "rfprint \"www.1-1.168.1.12\""
        mtfterm.sendline(command);
	time.sleep(50);
        mtfterm.readline();
	expectedLine = '[httpc] Error: No entry for host www.1-1.168.1.12 found'
	failMessage = 'testUrlValidation Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This tests case will check for request time out error 408 from server.
    def testRequestTimeOut(self):
	command =  "rfprint " + self.baseURL + "/408.php\"";
        mtfterm.sendline(command);
	time.sleep(1);
	expectedLine = '[rf] Error: Unexpected HTTP status code from server: 408'
	failMessage = 'testRequestTimeOut Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This tests case will check for conflict error 409 from server.
    def testErrorConflict(self):
	command =  "rfprint " + self.baseURL + "/409.php \"";
        mtfterm.sendline(command);
	time.sleep(1);
	expectedLine = '[rf] Error: Unexpected HTTP status code from server: 409'
	failMessage = 'testErrorConflict Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This tests case will check for server gone error 410 from server.
    def testServerGone(self):
	command =  "rfprint " + self.baseURL + "/410.php\"";
        mtfterm.sendline(command);
	time.sleep(1);
	expectedLine = '[rf] Error: Unexpected HTTP status code from server: 410'
	failMessage = 'testServerGone Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This tests case will check for internal service error 500 from server.
    def testInternelServerError(self):
	command =  "rfprint " + self.baseURL + "/500.php\"";
        mtfterm.sendline(command);
	time.sleep(1);
	expectedLine = '[rf] Error: Unexpected HTTP status code from server: 500'
	failMessage = 'testInternelServerError Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);

    #This tests case will check for service unavailable error 503 from server.
    def testUnavailableServiceError(self):
	command =  "rfprint " + self.baseURL + "/503.php\"";
        mtfterm.sendline(command);
	expectedLine = '[rf] Error: Unexpected HTTP status code from server: 503'
	failMessage = 'testUnavailableServiceError Test Failed'
	self.failIf(self.checkResult(expectedLine) != 1, failMessage);
