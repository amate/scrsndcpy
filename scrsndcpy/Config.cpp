#include "stdafx.h"
#include "Config.h"

#include <fstream>

#include "Utility\CodeConvert.h"
#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"

using json = nlohmann::json;
using namespace CodeConvert;

constexpr char obfuscation = 47;


bool Config::LoadConfig()
{
	std::ifstream fs((GetExeDirectory() / "setting.json").wstring());
	if (!fs) {
		return true;
	}
	json jsonSetting;
	fs >> jsonSetting;

	if (jsonSetting["Config"].is_null()) {
		return true;	// default
	}

	autoStart = jsonSetting["Config"].value("AutoStart", autoStart);
	loginPassword = jsonSetting["Config"].value("LoginPassword", "");
	for (char& c : loginPassword) {
		c ^= obfuscation;
	}

	// scrcpy
	maxSize = jsonSetting["Config"].value("MaxSize", maxSize);
	maxFPS = jsonSetting["Config"].value("MaxFPS", maxFPS);
	bitrate = jsonSetting["Config"].value("Bitrate", bitrate);
	turnScreenOff = jsonSetting["Config"].value("TurnScreenOff", turnScreenOff);

	// sndcpy
	bufferMultiple = jsonSetting["Config"].value("BufferMultiple", bufferMultiple);
	maxBufferSampleCount = 
		jsonSetting["Config"].value("MaxBufferSampleCount", maxBufferSampleCount);
	deviceMuteOnStart = jsonSetting["Config"].value("DeviceMuteOnStart", deviceMuteOnStart);

    return true;
}

void Config::SaveConfig()
{
	json jsonSetting;
	std::ifstream fs((GetExeDirectory() / "setting.json").wstring());
	if (fs) {
		fs >> jsonSetting;
	}
	fs.close();

	jsonSetting["Config"]["AutoStart"] = autoStart;

	std::string obfuPass;
	for (const char& c : loginPassword) {
		obfuPass.push_back(c ^ obfuscation);
	}
	jsonSetting["Config"]["LoginPassword"] = obfuPass;

	jsonSetting["Config"]["MaxSize"] = maxSize;
	jsonSetting["Config"]["MaxFPS"] = maxFPS;
	jsonSetting["Config"]["Bitrate"] = bitrate;
	jsonSetting["Config"]["TurnScreenOff"] = turnScreenOff;

	jsonSetting["Config"]["BufferMultiple"] = bufferMultiple;
	jsonSetting["Config"]["MaxBufferSampleCount"] = maxBufferSampleCount;
	jsonSetting["Config"]["DeviceMuteOnStart"] = deviceMuteOnStart;

	std::ofstream ofs((GetExeDirectory() / "setting.json").wstring());
	ofs << jsonSetting.dump(4);
}
