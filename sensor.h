#include <string>

class Sensor
{
public:
	std::string chip;
	std::string name;
	int id = 0;
	double reading = 0;

public:
	Sensor() = default;
	Sensor(std::string chipI, std::string nameI, int idI = 0): name(nameI), chip(chipI), name(nameI), id(idI) {}
	bool operator==(const Sensor& other) {return other.name == name && other.chip == chip;}
};
