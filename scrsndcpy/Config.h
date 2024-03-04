#pragma once

#include <string>

struct Config
{
	bool	autoStart = false;
	std::string	loginPassword;
	bool	autoWifiConnect = false;
	bool	reconnectOnResume = false;
	bool	enableUHID = false;
	bool	useScrcpyAudio = true;

	// scrcpy
	int		maxSize = 0;
	int		maxFPS = 0;
	int		bitrate = 0;
	bool	turnScreenOff = false;
	uint32_t	delayFrameCount = 0;
	uint32_t	videoBuffer_ms = 50;
	bool	noResize = false;

	// sndcpy
	int		bufferMultiple = 2;
	int		maxBufferSampleCount = 2000;
	bool	deviceMuteOnStart = true;
	bool	toggleMuteReverse = false;

	bool	LoadConfig();
	void	SaveConfig();
};

