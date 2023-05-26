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


#include "lm.h"
#include "pwmfan.h"

sig_atomic_t running = true;

void sig_handler(int sig)
{
	(void)sig;
	running = false;
}

bool quiet;

double fan_curve(double temperature, double min_fan, double max_fan, double low_temperature, double high_temperature)
{
	double slope = (max_fan-min_fan)/(high_temperature-low_temperature);
	return std::max(std::min(max_fan, min_fan+slope*(temperature-low_temperature)), min_fan);
}

double gpu_fan_zone(const std::vector<Sensor>& sensors)
{
	const char mi25Chip[] = "amdgpu-pci-4300";
	bool hitMi25 = false;
	const char monitored_sensor_name[] = "edge";

	double max_temp = std::numeric_limits<double>::min();
	for(const Sensor& sensor : sensors)
	{
		if(sensor.chip == mi25Chip && sensor.name == monitored_sensor_name)
		{
			hitMi25 = true;
			if(max_temp < sensor.reading)
				max_temp = sensor.reading;
		}
	}
	if(!hitMi25)
	{
		std::cerr<<"Could not get temperature from MI25 Ramping fans to maximum\n";
		return 1.0;
	}
	else
		return fan_curve(max_temp, 0.10, 1.0, 45, 75);
}

int main_loop()
{
	PwmFan gpuFan("/sys/class/hwmon/hwmon4/", 2);
	if(!gpuFan.ready())
	{
		std::cerr<<"Could not open pwm2 on /sys/class/hwmon/hwmon4/\n";
		return 1;
	}

	int ret = sensors_init(nullptr);
	if(ret < 0)
	{
		std::cerr<<"Could not init lm_sensors\n";
		gpuFan.setSpeed(1.0);
		return 1;
	}

	std::vector<const sensors_chip_name*> lm_chips = lm_get_chips("amdgpu-*");

	if(lm_chips.size() < 1)
	{
		std::cerr<<"Could not get gpus sensor";
		gpuFan.setSpeed(1.0);
		sensors_cleanup();
		return 1;
	}


	while(running)
	{
		std::vector<Sensor> sensors = lm_get_temperatures(lm_chips);
		double fanZone = gpu_fan_zone(sensors);

		if(!quiet)
		{
			for(const Sensor& sensor : sensors)
				std::cout<<"Sensor "<<sensor.chip<<':'<<sensor.name<<"\t= "<<sensor.reading<<'\n';
			std::cout<<"target gpu fan speed "<<fanZone*100<<"%\n";
		}

		gpuFan.setSpeed(fanZone);
		sleep(10);
	}

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

