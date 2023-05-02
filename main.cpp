#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sensors/sensors.h>
#include <sensors/error.h>
#include <signal.h>

#include "ipmi.h"
#include "lm.h"

sig_atomic_t running = true;

void sig_handler(int sig)
{
	(void)sig;
	running = false;
}

std::vector<Sensor> gather_sensors(std::vector<Sensor>& ipmi_sensors, ipmi_monitoring_ctx_t ctx, std::vector<const sensors_chip_name*>& lm_chips)
{
	std::vector<Sensor> out;
	struct ipmi_monitoring_ipmi_config ipmi_config = {};
	ipmi_config.driver_type = IPMI_MONITORING_DRIVER_TYPE_OPENIPMI;

	bool grabids = false;
	for(Sensor& sensor : ipmi_sensors)
	{
		if(sensor.id <= 0)
		{
			grabids = true;
			break;
		}
	}

	if(grabids)
	{
		if(!ipmi_fill_sensor_ids(ipmi_sensors, ctx, &ipmi_config))
		{
			std::cout<<"could not get ids for all the required sensors\n";
			return out;
		}
	}
	else
	{
		ipmi_update_sensors(ipmi_sensors, ctx, &ipmi_config);
	}

	out.insert(out.end(), ipmi_sensors.begin(), ipmi_sensors.end());
	std::vector<Sensor> lm_sensors = lm_get_temperatures(lm_chips);
	out.insert(out.end(), lm_sensors.begin(), lm_sensors.end());

	return out;
}

std::vector<double> get_fan_zones(const std::vector<Sensor>& sensors)
{
	std::vector<double> out;

	for(const Sensor& sensor : sensors)
		std::cout<<sensor.chip<<' '<<sensor.name<<": "<<sensor.reading;

	return out;
}

int main (int argc, char **argv)
{
	signal(SIGABRT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);

	int ret = sensors_init(nullptr);
	if(ret < 0)
	{
		std::cerr<<"Could not init lm_sensors\n";
	}

	std::vector<const sensors_chip_name*> lm_chips = lm_get_chips("amdgpu-*");
	std::vector<Sensor> ipmi_sensors;
	ipmi_sensors.push_back(Sensor("IPMI", "CPU Temp"));
	ipmi_sensors.push_back(Sensor("IPMI", "System Temp"));

	ipmi_monitoring_ctx_t ctx = init_ipmi_monitoring();
	if(!ctx)
		return 1;

	while(running)
	{
		std::vector<Sensor> sensors = gather_sensors(ipmi_sensors, ctx, lm_chips);
		get_fan_zones(sensors);
		sleep(1);
	}

	ipmi_monitoring_ctx_destroy(ctx);
	sensors_cleanup();
	return 0;
}

