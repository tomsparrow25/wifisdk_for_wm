#!/bin/bash
# Usage: test.sh <hostname>

CURL=`which curl`
if [ "$CURL" = "" ]; then
	echo "ERROR: curl is required to run tests"
	exit 1
fi

TESTHOST="localhost"
if [ "$1" != "" ]; then
	TESTHOST="$1"
fi

if [ "$TESTHOST" = "localhost" ]; then
	HTTPD="./httpd"
	TESTFILES="./testfiles"
	if [ ! -x $HTTPD -o ! -d $TESTFILES ]; then
		echo "ERROR: httpd must exist and must be in the current directory."
		exit 1
	fi

	# launch httpd
	$HTTPD $TESTFILES > /dev/null 2> /dev/null &
	if [ $? != 0 ]; then
		echo "ERROR: failed to launch httpd"
		exit 1
	fi
	PID=$!
	# wait for server start
	sleep 0.2
fi

# $1 -> Test title
# $2 -> Expected file size
# $3.... -> curl args
function runtest() {
	echo "========================================="
	echo -n $1
	EXPECTED_SIZE=$2
	shift 2
	$CURL $* > /tmp/foo 2> /tmp/curloutput
	RES=$?
	SIZE=`stat --format='%s' /tmp/foo`
	if [  "$RES" == "0" -a "$SIZE" == "$EXPECTED_SIZE" ]; then
		echo "PASS"
	else
		echo "FAIL"
		echo "curl output:"
		cat /tmp/curloutput
		echo "fetched file contents:"
		cat /tmp/foo
	fi
	rm /tmp/foo /tmp/curloutput
}

# Test handlers
runtest "Testing valid handler..." 64 $TESTHOST/memdump
runtest "Testing invalid handler..." 0 $TESTHOST/foobar
runtest "Testing substring of a handler..." 0 $TESTHOST/mem
echo "========================================="

if [ "$TESTHOST" = "localhost" ]; then
	# kill httpd
	kill $PID > /dev/null 2> /dev/null
fi
