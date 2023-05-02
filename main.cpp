#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sensors/sensors.h>
#include <sensors/error.h>

#include "ipmi.h"

std::vector<const sensors_chip_name*> lm_get_chips(const std::string& match)
{
	std::vector<const sensors_chip_name*> out;

	sensors_chip_name name_glob;
	int ret = sensors_parse_chip_name(match.c_str(), &name_glob);
	if(ret < 0)
	{
		std::cerr<<"could not parse chip name\n";
		return out;
	}

	const sensors_chip_name* s_name;
	int nr = 0;
	do
	{
		s_name = sensors_get_detected_chips(&name_glob, &nr);
		if(s_name)
			out.push_back(s_name);
	} while(s_name);
	sensors_free_chip_name(&name_glob);

	return out;
}

std::vector<Sensor> lm_get_sensors(std::vector<const sensors_chip_name*>& chips)
{
	std::vector<Sensor> out;

	for(const sensors_chip_name* chip : chips)
	{
		int nr = 0;
		const sensors_feature* feature;
		while((feature = sensors_get_features(chip, &nr)))
		{
			if(feature->type != SENSORS_FEATURE_TEMP)
				continue;
			const sensors_subfeature* subfeature = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
			if(!subfeature)
			{
				std::cerr<<"could not read subfeature\n";
				continue;
			}

			double val = 0;
			int ret = sensors_get_value(chip, subfeature->number, &val);
			if(ret < 0)
			{
				std::cerr<<"unable to read temperature for subfeature "
					<<subfeature->number<<' '<<subfeature->name<<": "<<sensors_strerror(ret)<<'\n';
				continue;
			}
			Sensor sensor;
			sensor.name.assign(sensors_get_label(chip, feature));
			sensor.id = subfeature->number;
			sensor.reading = val;
			out.push_back(sensor);
		}
	}
	return out;
}

int main (int argc, char **argv)
{

	int ret = sensors_init(nullptr);
	if(ret < 0)
	{
		std::cerr<<"Could not init lm_sensors\n";
	}

	std::vector<const sensors_chip_name*> lm_chips = lm_get_chips("amdgpu-*");
	std::vector<Sensor> lmsensors = lm_get_sensors(lm_chips);

	for(const Sensor& sensor : lmsensors)
	{
		std::cout<<sensor.name<<": "<<sensor.reading<<'\n';
	}

	struct ipmi_monitoring_ipmi_config ipmi_config = {};
	ipmi_config.driver_type = IPMI_MONITORING_DRIVER_TYPE_OPENIPMI;

	std::vector<Sensor> sensors;
	sensors.push_back(Sensor("CPU Temp"));
	sensors.push_back(Sensor("System Temp"));

	ipmi_monitoring_ctx_t ctx = init_ipmi_monitoring();
	if(!ctx)
		return 1;

	if(!ipmi_fill_sensor_ids(sensors, ctx, &ipmi_config))
	{
		std::cout<<"could not get ids for all the required sensors\n";
		return 1;
	}

	while(true)
	{
		ipmi_update_sensors(sensors, ctx, &ipmi_config);
		std::cout<<sensors[0].reading<<std::endl;
		sleep(1);
	}

	ipmi_monitoring_ctx_destroy(ctx);
	sensors_cleanup();
	return 0;
}

