#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <sensors/sensors.h>
#include <sensors/error.h>

#include "sensor.h"

std::vector<const sensors_chip_name*> lm_get_chips(const std::string& match);
std::vector<Sensor> lm_get_temperatures(std::vector<const sensors_chip_name*>& chips);
