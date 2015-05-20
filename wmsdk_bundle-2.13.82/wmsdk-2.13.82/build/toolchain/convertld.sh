#/bin/sh
arm-none-eabi-ld `echo $@ | sed -e 's/\/cygdrive\/\([a-zA-Z]\)\//\1:\//g'`
