#include <vector>
#include <string>
#include <ipmi_monitoring.h>
#include <freeipmi/freeipmi.h>

#include "sensor.h"

Sensor ipmi_read_sensor_from_ctx(ipmi_monitoring_ctx_t ctx);

bool ipmi_fill_sensor_ids(std::vector<Sensor>& sensors, ipmi_monitoring_ctx_t ctx, struct ipmi_monitoring_ipmi_config* config);

bool ipmi_update_sensors(std::vector<Sensor>& sensors, ipmi_monitoring_ctx_t ctx, struct ipmi_monitoring_ipmi_config* config);

ipmi_monitoring_ctx_t init_ipmi_monitoring();

ipmi_ctx_t ipmi_open_context();

bool ipmi_set_fan_group(ipmi_ctx_t raw_ctx, uint8_t group, double speed);
