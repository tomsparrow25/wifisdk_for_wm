#!/bin/sh

# Script to merge all certificates in a single pem file.

HELP_TEXT=\
"Merges all WPA2 Enterprise certificates and network parameters in a single \
file named cert.img. Also script can generate header files cacert.h, \
client-cert.h and client-key.h, each for respective certificates and nw-params.h
containing macros for network Name, SSID and Identity.
Example: ./merge.sh -c <ca_cert_file> -l <client_cert_file> -k \
<client_key_file> -n <network_parameter_file>

Options: \n
  -c, --ca-cert       Indicates CA certificate.\n
  -l, --client-cert   Indicates Client certificate.\n
  -k, --client-key    Indicates Client key.\n
  -n, --nw-params     Indicates Network parameter file.\n
  -o, --out	      Optional: Specify type of output file to be generated.\n
		      Valid parameters are \"header\" or \"img\" to generate\n
		      corresponding file. When -o is not specified,by default,\n
		      script generates both type of files.
  -h, --help          Help.\n
"
# Define constants
DELIM="\t\n"
IMG_FILE="cert.img"
CA_HEADER="cacert.h"
CLIENT_CERT_HEADER="client-cert.h"
CLIENT_KEY_HEADER="client-key.h"
NW_PARAM_HEADER="nw-params.h"
OUTPUT_DIR=./output
SOURCE_DIR=`readlink -f $PWD/../`

if [ $# -lt 8 ]
then
	echo -e -n "$HELP_TEXT"
	exit 1
fi

# Check if output directory exist. If not, create one.
if [ ! -d $OUTPUT_DIR ]
then
	mkdir $OUTPUT_DIR
fi

# By defult generate both header and pem files
HEADER=""
PEM=""

set -- `getopt -n$0 -u -a --longoptions="ca-cert: client-cert: client-key: \
nw-params: out:" "h: c: l: k: n: o:" "$@"` || echo -e "HELP_TEXT"
[ $# -eq 0 ] && echo -e "HELP_TEXT"

while [ $# -gt 0 ]
do
    case "$1" in
       -c|--ca-cert)	 cp $2 ca_cert.tmp ; shift ;;
       -l|--client-cert) cp $2 client_cert.tmp ; shift ;;
       -k|--client-key)	 cp $2 client_key.tmp ; shift ;;
       -n|--nw-params)   cp $2 nw_params.tmp ; shift ;;
       -o|--out)	 if [ "$2" = "header" ]; then
				HEADER='y';
			 elif [ "$2" = "img" ]; then
				IMG='y';
			 else
				HEADER=n;
				PEM=n;
				echo "Specify correct output file";
			 fi
			 shift ;;
       -h|--help) 	 echo -e " -h $HELP_TEXT" ;;
       --)               shift ; break ;;
       -*)               echo -e " -* $HELP_TEXT" ;;
       *)                break ;;
    esac
    shift
done

if [ "$HEADER" = "" ] && [ "$IMG" = "" ]; then
	HEADER=y
	IMG=y
fi

if [ "$IMG" == 'y' ]
then
# Create IMG file
# Get a paragraph that has BEGIN keyword.
sed -n '/BEGIN/,/END/p' ca_cert.tmp > $OUTPUT_DIR/$IMG_FILE
# Add delimiters
echo -e -n "$DELIM" >> $OUTPUT_DIR/$IMG_FILE
# Get a paragraph that has BEGIN keyword.
sed -n '/BEGIN/,/END/p' client_cert.tmp >> $OUTPUT_DIR/$IMG_FILE
# Add delimiters
echo -e -n "$DELIM" >> $OUTPUT_DIR/$IMG_FILE
# Get a paragraph that has BEGIN keyword.
sed -n '/BEGIN/,/END/p' client_key.tmp >> $OUTPUT_DIR/$IMG_FILE
# Add delimiters
echo -e -n "$DELIM" >> $OUTPUT_DIR/$IMG_FILE

#Append network parameters
name=`grep "name" < nw_params.tmp | cut -f2 -d=`
echo -e -n "$name$DELIM" >> $OUTPUT_DIR/$IMG_FILE
identity=`grep "identity" < nw_params.tmp | cut -f2 -d=`
echo -e -n "$identity$DELIM" >> $OUTPUT_DIR/$IMG_FILE
ssid=`grep "ssid" < nw_params.tmp | cut -f2 -d=`
echo -e -n "$ssid$DELIM" >> $OUTPUT_DIR/$IMG_FILE

echo ""
echo "###########################################"
echo "Merged certificate is now ready!"
echo "You can burn the certificate using flashprog utility as:"
echo " # cd /path/to/WMSDK/tools/mc200/OpenOCD"
echo " # ./flashprog.sh --cert $PWD/output/$IMG_FILE"
echo "###########################################"
echo ""
fi

if [ "$HEADER" == 'y' ]
then
# Create header files
# Define array for CA certificate
echo -e "static unsigned char ca_cert[] = { \"\\" > \
	$OUTPUT_DIR/$CA_HEADER
# Append CA certificate
sed -i -n '/BEGIN/,/END/p' ca_cert.tmp
sed 's/$/\\n\\/' ca_cert.tmp >> $OUTPUT_DIR/$CA_HEADER
echo -e "\" };\n" >> $OUTPUT_DIR/$CA_HEADER
echo "unsigned int ca_cert_size = sizeof(ca_cert);" >> \
	$OUTPUT_DIR/$CA_HEADER

# Define array for Client certificate
echo -e "static unsigned char client_cert[] = { \"\\" > \
	$OUTPUT_DIR/$CLIENT_CERT_HEADER
#Append Client certificate
sed -i -n '/BEGIN/,/END/p' client_cert.tmp
sed 's/$/\\n\\/' client_cert.tmp >> $OUTPUT_DIR/$CLIENT_CERT_HEADER
echo -e "\" };\n" >> $OUTPUT_DIR/$CLIENT_CERT_HEADER
echo "unsigned int client_cert_size = sizeof(client_cert);" >> \
	$OUTPUT_DIR/$CLIENT_CERT_HEADER

# Define array for Client key
echo -e "static unsigned char client_key[] = { \"\\" > \
	$OUTPUT_DIR/$CLIENT_KEY_HEADER
#Append Client key
sed -i -n '/BEGIN/,/END/p' client_key.tmp
sed 's/$/\\n\\/' client_key.tmp >> $OUTPUT_DIR/$CLIENT_KEY_HEADER
echo -e "\" };\n" >> $OUTPUT_DIR/$CLIENT_KEY_HEADER
echo "unsigned int client_key_size = sizeof(client_key);" >> \
	$OUTPUT_DIR/$CLIENT_KEY_HEADER

# Header file for network parameters
name=`grep "name" < nw_params.tmp | cut -f2 -d=`
identity=`grep "identity" < nw_params.tmp | cut -f2 -d=`
ssid=`grep "ssid" < nw_params.tmp | cut -f2 -d=`
echo "#define WPA2_NW_NAME	\"$name\"" > $OUTPUT_DIR/$NW_PARAM_HEADER
echo "#define WPA2_NW_IDENTITY	\"$identity\"" >> $OUTPUT_DIR/$NW_PARAM_HEADER
echo "#define WPA2_NW_SSID	\"$ssid\"" >> $OUTPUT_DIR/$NW_PARAM_HEADER

echo ""
echo "###########################################"
echo "Header files are ready!"
echo -e "Copy header files $CA_HEADER, $CLIENT_CERT_HEADER, $CLIENT_KEY_HEADER \
, $NW_PARAM_HEADER"
echo "from $PWD/output directory to $SOURCE_DIR directory"
echo "Rebuild the sources with these header files to run the demo successfully"
echo "###########################################"
echo ""
fi
# Delete all temp files
rm -rf *.tmp
