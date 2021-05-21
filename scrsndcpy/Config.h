#pragma once

#include <string>

struct Config
{
	bool	autoStart = false;
	std::string	loginPassword;
	bool	autoWifiConnect = false;

	// scrcpy
	int		maxSize = 0;
	int		maxFPS = 0;
	int		bitrate = 0;
	bool	turnScreenOff = true;
	uint32_t	delayFrameCount = 0;

	// sndcpy
	int		bufferMultiple = 2;
	int		maxBufferSampleCount = 2000;
	bool	deviceMuteOnStart = true;

	bool	LoadConfig();
	void	SaveConfig();
};

