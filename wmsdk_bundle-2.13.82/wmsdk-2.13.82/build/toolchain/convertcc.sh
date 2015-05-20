#/bin/sh
arm-none-eabi-gcc `echo $@ | sed -e 's/\/cygdrive\/\([a-zA-Z]\)\//\1:\//g'`
