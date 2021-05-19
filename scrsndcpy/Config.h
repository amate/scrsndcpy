#pragma once

#include <string>

struct Config
{
	bool	autoStart = false;
	std::string	loginPassword;

	// scrcpy
	int		maxSize = 0;
	int		maxFPS = 0;
	int		bitrate = 0;
	bool	turnScreenOff = true;

	// sndcpy
	int		bufferMultiple = 2;
	int		maxBufferSampleCount = 2000;
	bool	deviceMuteOnStart = true;

	bool	LoadConfig();
	void	SaveConfig();
};

