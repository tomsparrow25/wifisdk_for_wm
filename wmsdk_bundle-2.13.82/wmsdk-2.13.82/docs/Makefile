PLANTUML_JAR ?= $(wildcard ~/build-deps/plantuml.jar)
COPY_CMD = $(shell which cp | tail -1 )

INPUT_FILES =  src/incl/platform/os/freertos/wm_os.h \
	   src/incl/platform/net/lwip/wm_net.h \
	   src/incl/sdk/drivers/wlan/wifi-decl.h \
	   src/incl/sdk/drivers/wlan/wifi_events.h \
	   src/incl/sdk/wlan.h \
	   src/incl/sdk/wlan_11d.h \
	   src/incl/sdk/pwrmgr.h \
	   src/incl/sdk/psm.h \
	   src/incl/sdk/ftfs.h \
	   src/incl/sdk/wmstdio.h \
	   src/incl/sdk/cli.h \
	   src/incl/sdk/mdev.h \
	   src/incl/sdk/provisioning.h \
	   src/incl/sdk/healthmon.h \
	   src/incl/sdk/wps.h \
	   src/incl/sdk/p2p.h \
	   src/incl/sdk/httpd.h \
	   src/incl/sdk/rfget.h \
	   src/incl/sdk/partition.h \
	   src/incl/sdk/dhcp-server.h \
	   src/incl/sdk/wmtime.h \
	   src/incl/sdk/wmcrypto.h \
	   src/incl/sdk/drivers/mdev_crc.h \
	   src/incl/sdk/drivers/mdev_aes.h \
	   src/incl/sdk/drivers/mdev_gpio.h \
	   src/incl/sdk/drivers/mdev_i2c.h \
	   src/incl/sdk/drivers/mdev_rtc.h \
	   src/incl/sdk/drivers/mdev_wdt.h \
	   src/incl/sdk/drivers/mdev_gpt.h \
	   src/incl/sdk/drivers/mdev_uart.h \
	   src/incl/sdk/drivers/mdev_pinmux.h \
	   src/incl/sdk/drivers/mdev_acomp.h \
	   src/incl/sdk/drivers/mdev_adc.h \
	   src/incl/sdk/drivers/mdev_dac.h \
	   src/incl/sdk/drivers/mdev_ssp.h \
	   src/incl/sdk/drivers/mdev_sdio.h \
	   src/incl/sdk/flash.h \
	   src/incl/sdk/drivers/peripherals/lcd.h \
	   src/incl/sdk/drivers/peripherals/dtp_drv.h \
	   src/incl/sdk/telnetd.h \
	   src/incl/sdk/json.h \
	   src/incl/sdk/ezxml.h \
	   src/incl/sdk/httpc.h \
	   src/incl/sdk/wmstats.h \
	   src/incl/sdk/app_framework.h \
	   src/incl/sdk/mdns.h \
	   src/incl/sdk/wm-tls.h \
	   src/incl/sdk/board.h \
           src/incl/sdk/wmsysinfo.h \
	   src/incl/platform/arch/arch-mc200/sys.h  \
	   src/incl/platform/arch/arch-mc200/pm_mc200.h  \
	   src/incl/sdk/drivers/mdev_usb.h \
	   src/incl/sdk/overlays.h \
	   src/incl/sdk/drivers/mdev_usb_host.h \
	   docs/README

ARRAYENT_FILES = external/arrayent/source/include/aca.h

XIVELY_FILES =  external/libxively/src/libxively/xi_err.h \
	   external/libxively/src/libxively/xively.h
.PHONY: all clean xively clean_arrayent clean_xively
all: xively arrayent
	mkdir -p gen/
	/bin/cp doxy.conf gen/doxy.conf
	chmod +w gen/doxy.conf
	echo 'INPUT=$(INPUT_FILES)' >> gen/doxy.conf
ifneq ($(PLANTUML_JAR),)
# Enable uml_diag only if the images can be generated
	echo 'ENABLED_SECTIONS=uml_diag' >> gen/doxy.conf
	echo 'IMAGE_PATH=docs/gen/images' >> gen/doxy.conf
	mkdir -p gen/images
	cd .. ; java -jar $(PLANTUML_JAR) -v -o `pwd`/docs/gen/images $(INPUT_FILES)
endif
	cd .. ; doxygen docs/gen/doxy.conf
	$(COPY_CMD) -f utils/logo.gif refman/html/
	$(COPY_CMD) -f utils/doxygen.sty utils/Marvell_logo_spot_PMS_cropped.png utils/Marvell_logotype_c_small.png refman/latex/
	$(MAKE) -C refman/latex/ refman.pdf
	mkdir -p refman/pdf/
	mv refman/latex/refman.pdf refman/pdf/wmsdk-reference-manual.pdf

xively:
	mkdir -p gen/
	/bin/cp doxy-xively.conf gen/doxy.conf
	chmod +w gen/doxy.conf
	echo 'INPUT=$(XIVELY_FILES)' >> gen/doxy.conf
ifneq ($(PLANTUML_JAR),)
# Enable uml_diag only if the images can be generated
	echo 'ENABLED_SECTIONS=uml_diag' >> gen/doxy.conf
	echo 'IMAGE_PATH=docs/gen/images' >> gen/doxy.conf
	mkdir -p gen/images
	cd .. ; java -jar $(PLANTUML_JAR) -v -o `pwd`/docs/gen/images $(INPUT_FILES)
endif
	cd .. ; doxygen docs/gen/doxy.conf
	$(COPY_CMD) -f utils/logo.gif refman/html/
	$(COPY_CMD) -f utils/doxygen.sty utils/Marvell_logo_spot_PMS_cropped.png utils/Marvell_logotype_c_small.png refman/latex/
	$(MAKE) -C refman/latex/ refman.pdf
	mkdir -p refman/pdf/
	mv refman/latex/refman.pdf refman/pdf/xively-reference-manual.pdf
arrayent:
	mkdir -p gen/
	/bin/cp doxy-arrayent.conf gen/doxy.conf
	chmod +w gen/doxy.conf
	echo 'INPUT=$(ARRAYENT_FILES)' >> gen/doxy.conf
ifneq ($(PLANTUML_JAR),)
# Enable uml_diag only if the images can be generated
	echo 'ENABLED_SECTIONS=uml_diag' >> gen/doxy.conf
	echo 'IMAGE_PATH=docs/gen/images' >> gen/doxy.conf
	mkdir -p gen/images
	cd .. ; java -jar $(PLANTUML_JAR) -v -o `pwd`/docs/gen/images $(INPUT_FILES)
endif
	cd .. ; doxygen docs/gen/doxy.conf
	$(COPY_CMD) -f utils/logo.gif refman/html/
	$(COPY_CMD) -f utils/doxygen.sty utils/Marvell_logo_spot_PMS_cropped.png utils/Marvell_logotype_c_small.png refman/latex/
	$(MAKE) -C refman/latex/ refman.pdf
	mkdir -p refman/pdf/
	mv refman/latex/refman.pdf refman/pdf/arrayent-reference-manual.pdf
clean:
	$(AT)rm -fr ./refman/ ./gen
clean_xively:
	$(AT)rm -fr ./refman/pdf/xively* ./gen
clean_arrayent:
	$(AT)rm -fr ./refman/pdf/arrayent* ./gen

