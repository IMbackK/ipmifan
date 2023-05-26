#pragma once
#include <filesystem>

class PwmFan
{
private:
	int pwmFileFd;
	int pwmstatusFileFd;

public:
	PwmFan(const std::filesystem::path& hwmonDirI, int pwmNumber);
	~PwmFan();
	bool isSpinning();
	float getSpeed();
	bool setSpeed(float speed);
	bool ready();
};
