deps_config := \
	src/middleware/json/Kconfig \
	src/middleware/httpd/Kconfig \
	src/middleware/tls/Kconfig \
	src/middleware/mdns/Kconfig \
	src/wlan/Kconfig \
	src/app_framework/Kconfig \
	./build/config/Kconfig

.config include/autoconf.h: $(deps_config)

$(deps_config):
