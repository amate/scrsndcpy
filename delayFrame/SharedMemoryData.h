#pragma once

#include <stdint.h>

enum CopyDataID {
	kPutLog = 1,
};

enum DelayFrameCommand {
	WM_DELAYFRAME_COMMAND = WM_APP + 3,
	kStartSndcpy = 1,
	kRestartSndcpy,
	kWakefulness,

	kHankakuZenkaku,
	kCtrlEnter,
};

struct SharedMemoryData
{
	uint32_t	delayFrameCount;
	bool		doEventFlag;

	HWND	hwndMainDlg;

	bool	streamingReady;
	bool	simpleAudioReady;
	int		bufferMultiple;
	int		maxBufferSampleCount;
	int		playSoundVolume;
	bool	nowSoundStreaming;
};
