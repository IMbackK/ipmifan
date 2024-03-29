#include <cstddef>
#include <cstdint>
#include <freeipmi/api/ipmi-api.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sensors/sensors.h>
#include <sensors/error.h>
#include <signal.h>
#include <limits>
#include <freeipmi/freeipmi.h>

#include "ipmi.h"
#include "lm.h"

sig_atomic_t running = true;

void sig_handler(int sig)
{
	(void)sig;
	running = false;
}

bool quiet;

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

double fan_curve(double temperature, double min_fan, double max_fan, double low_temperature, double high_temperature)
{
	double slope = (max_fan-min_fan)/(high_temperature-low_temperature);
	return std::max(std::min(max_fan, min_fan+slope*(temperature-low_temperature)), min_fan);
}

double gpu_fan_zone(const std::vector<Sensor>& sensors)
{
	std::vector<std::pair<std::string, bool>> gpus = {{"amdgpu-pci-0300", false}, {"amdgpu-pci-8300", false}, {"amdgpu-pci-8900", false}};
	const char monitored_sensor_name[] = "edge";

	double max_temp = std::numeric_limits<double>::min();
	for(const Sensor& sensor : sensors)
	{
		if(sensor.name == monitored_sensor_name)
		{
			for(std::pair<std::string, bool>& gpu : gpus)
			{
				if(sensor.chip == gpu.first)
				{
					if(max_temp < sensor.reading)
						max_temp = sensor.reading;
					gpu.second = true;
				}
			}
		}
	}
	for(std::pair<std::string, bool>& gpu : gpus)
	{
		if(!gpu.second)
		{
			std::cerr<<"Could not get temperature from "<<gpu.first<<" ramping fans to maximum\n";
			return 1.0;
		}
	}

	return fan_curve(max_temp, 0.05, 1.0, 45, 75);
}

double system_fan_zone(const std::vector<Sensor>& sensors)
{
	Sensor cpu("IPMI", "CPU Temp");
	Sensor system("IPMI", "System Temp");
	bool hitCpu = false;
	bool hitSystem = false;
	std::vector<double> out;

	for(const Sensor& sensor : sensors)
	{
		if(cpu == sensor)
		{
			hitCpu = true;
			cpu = sensor;
		}
		else if(sensor == system)
		{
			hitSystem = true;
			system = sensor;
		}
	}

	if(hitCpu && hitSystem)
	{
		double fanSystem = fan_curve(system.reading, 0.33, 1.0, 40, 65);
		double fanCpu = fan_curve(cpu.reading, 0.33, 1.0, 40, 70);

		return std::max(fanSystem, fanCpu);
	}
	else
	{
		std::cerr<<"Could not get temperature from System or Cpu! Ramping fans to maximum\n";
		return 1;
	}
}

std::vector<double> get_fan_zones(const std::vector<Sensor>& sensors)
{
	std::vector<double> out;
	out.push_back(system_fan_zone(sensors));
	out.push_back(gpu_fan_zone(sensors));
	return out;
}

int main_loop()
{
	ipmi_ctx_t raw_ctx = ipmi_open_context();
	if(!raw_ctx)
	{
		sensors_cleanup();
		return 1;
	}

	int ret = sensors_init(nullptr);
	if(ret < 0)
	{
		std::cerr<<"Could not init lm_sensors\n";
		ipmi_set_fan_group(raw_ctx, 0, 1);
		ipmi_set_fan_group(raw_ctx, 1, 1);
		ipmi_ctx_close(raw_ctx);
		ipmi_ctx_destroy(raw_ctx);
		return 1;
	}

	std::vector<const sensors_chip_name*> lm_chips = lm_get_chips("amdgpu-*");
	std::vector<Sensor> ipmi_sensors;
	ipmi_sensors.push_back(Sensor("IPMI", "CPU Temp"));
	ipmi_sensors.push_back(Sensor("IPMI", "System Temp"));

	if(lm_chips.size() < 2)
	{
		std::cerr<<"Could not get both monitored gpus!";
		ipmi_set_fan_group(raw_ctx, 0, 1);
		ipmi_set_fan_group(raw_ctx, 1, 1);
		ipmi_ctx_close(raw_ctx);
		ipmi_ctx_destroy(raw_ctx);
		sensors_cleanup();
		return 1;
	}

	ipmi_monitoring_ctx_t monitoring_ctx = init_ipmi_monitoring();
	if(!monitoring_ctx)
	{
		ipmi_set_fan_group(raw_ctx, 0, 1);
		ipmi_set_fan_group(raw_ctx, 1, 1);
		ipmi_ctx_close(raw_ctx);
		ipmi_ctx_destroy(raw_ctx);
		sensors_cleanup();
		return 1;
	}


	while(running)
	{
		std::vector<Sensor> sensors = gather_sensors(ipmi_sensors, monitoring_ctx, lm_chips);
		std::vector<double> fanzones = get_fan_zones(sensors);

		if(!quiet)
		{
			for(const Sensor& sensor : sensors)
				std::cout<<"Sensor "<<sensor.chip<<':'<<sensor.name<<"\t= "<<sensor.reading<<'\n';
			for(size_t i = 0; i < fanzones.size(); ++i)
				std::cout<<"setting fan group "<<i<<" to "<<fanzones[i]*100<<"%\n";
		}

		ipmi_set_fan_group(raw_ctx, 0, fanzones[0]);
		ipmi_set_fan_group(raw_ctx, 1, fanzones[1]);
		sleep(10);
	}

	ipmi_set_fan_group(raw_ctx, 0, 1);
	ipmi_set_fan_group(raw_ctx, 1, 1);
	ipmi_ctx_close(raw_ctx);
	ipmi_ctx_destroy(raw_ctx);
	ipmi_monitoring_ctx_destroy(monitoring_ctx);
	sensors_cleanup();

	return 0;
}

int main (int argc, char **argv)
{
	signal(SIGABRT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);

	if(argc > 1)
		quiet = true;

	int ret = 0;
	for(size_t i = 0; i < 3; ++i)
	{
		ret = main_loop();
		if(!running)
			break;
		std::cerr<<"Mainloop unable to start, retrying in 10 sec\n";
		sleep(10);
	}

	if(ret != 0)
		std::cerr<<"Error not clearing, giveing up\n";

	return ret;
}

