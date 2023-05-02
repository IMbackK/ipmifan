#include "lm.h"
#include <sensors/sensors.h>

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

std::vector<Sensor> lm_get_temperatures(std::vector<const sensors_chip_name*>& chips)
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

			char name[256];
			sensors_snprintf_chip_name(name, sizeof(name), chip);

			Sensor sensor;
			sensor.chip.assign(name);
			sensor.name.assign(sensors_get_label(chip, feature));
			sensor.id = subfeature->number;
			sensor.reading = val;
			out.push_back(sensor);
		}
	}
	return out;
}
