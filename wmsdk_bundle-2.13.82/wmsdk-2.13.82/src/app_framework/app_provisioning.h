#ifndef APP_PROVISIONING_H_
#define APP_PROVISIONING_H_

#include <provisioning.h>


/** Interrupt the provisioning process */
void app_interrupt_provisioning();

void display_provisioning_pin();
int app_prov_httpd_init(void);
int app_prov_wps_session_req(char *pin);

int app_set_scan_config_psm(struct prov_scan_config *scancfg);

int app_load_scan_config(struct prov_scan_config *scancfg);

int app_provisioning_get_type();

#endif				/*APP_PROVISIONING_H_ */
