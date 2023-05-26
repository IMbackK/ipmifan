#include "pwmfan.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

PwmFan::PwmFan(const std::filesystem::path& hwmonDir, int pwmNumber)
{
	std::filesystem::path pwmFilename = hwmonDir / ("pwm" + std::to_string(pwmNumber));
	std::filesystem::path pwmStatusFilename = hwmonDir / ("pwm" + std::to_string(pwmNumber) + "_enable");
	pwmFileFd = open(pwmFilename.c_str(), O_WRONLY);
	pwmstatusFileFd = open(pwmStatusFilename.c_str(), O_WRONLY);
	write(pwmstatusFileFd, "1", 1);
}

PwmFan::~PwmFan()
{
	write(pwmstatusFileFd, "2", 1);
	if(pwmFileFd!= -1)
		close(pwmFileFd);
	if(pwmstatusFileFd!= -1)
		close(pwmstatusFileFd);
}

bool PwmFan::ready()
{
	return pwmFileFd != -1 && pwmstatusFileFd != -1;
}

bool PwmFan::isSpinning()
{
	return getSpeed() > 0;
}

float PwmFan::getSpeed()
{
	char buffer[256];
	lseek(pwmFileFd, 0, SEEK_SET);
	if(!read(pwmFileFd, &buffer, sizeof(char)))
		return -1;
	unsigned int pwmValue = std::atoi(buffer);
	return 100.0 * pwmValue / 255;
}

bool PwmFan::setSpeed(float speed)
{
	if (speed < 0 || speed > 1)
		return false;
	unsigned int pwmValue = static_cast<unsigned int>(speed*255);
	lseek(pwmFileFd, 0, SEEK_SET);
	std::string buffer = std::to_string(pwmValue);
	if(!write(pwmFileFd, buffer.data(), buffer.size()))
		return false;
	return true;
}
