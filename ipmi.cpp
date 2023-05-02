#include "ipmi.h"
#include <algorithm>
#include <iostream>

static double ipmi_convert_sensor_reading(void *sensor_reading, int sensor_reading_type)
{
	if(sensor_reading_type == IPMI_MONITORING_SENSOR_READING_TYPE_UNSIGNED_INTEGER8_BOOL)
		return (double)(*((uint8_t *)sensor_reading));
	else if (sensor_reading_type == IPMI_MONITORING_SENSOR_READING_TYPE_UNSIGNED_INTEGER32)
		return (double)(*((uint32_t *)sensor_reading));
	else if (sensor_reading_type == IPMI_MONITORING_SENSOR_READING_TYPE_DOUBLE)
		return *((double *)sensor_reading);
	return 0;
}

Sensor ipmi_read_sensor_from_ctx(ipmi_monitoring_ctx_t ctx)
{
	Sensor sensor;
	int record_id = ipmi_monitoring_sensor_read_record_id(ctx);
	int reading_type = ipmi_monitoring_sensor_read_sensor_reading_type(ctx);
	char *name = ipmi_monitoring_sensor_read_sensor_name(ctx);
	void *reading = ipmi_monitoring_sensor_read_sensor_reading(ctx);

	if(record_id < 0 || reading_type < 0 || !name || !reading)
		return sensor;

	if(reading_type != IPMI_MONITORING_SENSOR_READING_TYPE_UNSIGNED_INTEGER32 &&
			reading_type != IPMI_MONITORING_SENSOR_READING_TYPE_DOUBLE)
		return sensor;

	sensor.name.assign(name);
	sensor.reading = ipmi_convert_sensor_reading(reading, reading_type);
	sensor.id = record_id;
	return sensor;
}

bool ipmi_fill_sensor_ids(std::vector<Sensor>& sensors, ipmi_monitoring_ctx_t ctx, struct ipmi_monitoring_ipmi_config* config)
{
	unsigned int types[] = {IPMI_MONITORING_SENSOR_TYPE_TEMPERATURE};
	unsigned int types_len = sizeof(types)/sizeof(types[0]);
	int sensor_count = ipmi_monitoring_sensor_readings_by_sensor_type(ctx, NULL, config, 0, types, types_len, NULL, NULL);

	for(int i = 0; i < sensor_count; ++i, ipmi_monitoring_sensor_iterator_next(ctx))
	{
		Sensor sensor = ipmi_read_sensor_from_ctx(ctx);

		auto search = std::find(sensors.begin(), sensors.end(), sensor);
		if(search != sensors.end())
			*search = sensor;
	}

	for(Sensor& sensor : sensors)
	{
		if(sensor.id == 0 && sensor.chip == "IPMI")
			return false;
	}

	return true;
}

bool ipmi_update_sensors(std::vector<Sensor>& sensors, ipmi_monitoring_ctx_t ctx, struct ipmi_monitoring_ipmi_config* config)
{
	unsigned int *ids = new unsigned int[sensors.size()];
	for(size_t i = 0; i < sensors.size(); ++i)
		ids[i] = sensors[i].id;

	int sensor_count = ipmi_monitoring_sensor_readings_by_record_id(ctx, NULL, config, 0, ids, sensors.size(), NULL, NULL);
	delete[] ids;
	if(sensor_count < static_cast<int>(sensors.size()))
		return false;

	for(int i = 0; i < sensor_count; ++i, ipmi_monitoring_sensor_iterator_next(ctx))
	{
		Sensor readsensor = ipmi_read_sensor_from_ctx(ctx);

		for(Sensor& sensor : sensors)
		{
			if(sensor.id == readsensor.id)
				sensor = readsensor;
		}
	}
	return true;
}

ipmi_monitoring_ctx_t init_ipmi_monitoring()
{
	int errnum;
	int ret = ipmi_monitoring_init(/*IPMI_MONITORING_FLAGS_DEBUG*/ 0, &errnum);
	if(ret < 0)
	{
		std::cerr<<"Could not init ipmi "<<ipmi_monitoring_ctx_strerror(errnum)<<'\n';
		return NULL;
	}

	ipmi_monitoring_ctx_t ctx = ipmi_monitoring_ctx_create();
	if(!ctx)
	{
		std::cerr<<"failed to create montioring context\n";
		return NULL;
	}

	return ctx;
}
