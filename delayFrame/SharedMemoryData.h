#pragma once

#include <stdint.h>

struct SharedMemoryData
{
	uint32_t	delayFrameCount;
	bool		doEventFlag;
};
