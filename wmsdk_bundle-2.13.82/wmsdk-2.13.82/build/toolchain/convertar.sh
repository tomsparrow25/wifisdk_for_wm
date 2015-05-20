#/bin/sh
arm-none-eabi-ar `echo $@ | sed -e 's/\/cygdrive\/\([a-zA-Z]\)\//\1:\//g'`
