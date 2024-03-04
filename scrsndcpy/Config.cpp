#include "stdafx.h"
#include "Config.h"

#include <fstream>
#include <sstream>

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
	std::string obfuPass = jsonSetting["Config"].value("LoginPassword", "");
	if (obfuPass.length()) {
		ATLASSERT(obfuPass.length() % 3 == 0);
		for (LPCSTR str = obfuPass.c_str(); *str != '\0'; str += 3) {
			ATLASSERT(*str == '\\');
			unsigned int chex = 0;
			sscanf(&str[1], "%x", &chex);
			chex ^= obfuscation;
			loginPassword += static_cast<char>(chex);
		}

	}
	autoWifiConnect = jsonSetting["Config"].value("AutoWifiConnect", autoWifiConnect);
	reconnectOnResume = jsonSetting["Config"].value("ReconnectOnResume", reconnectOnResume);
	enableUHID = jsonSetting["Config"].value("EnableUHID", enableUHID);
	useScrcpyAudio = jsonSetting["Config"].value("UseScrcpyAudio", useScrcpyAudio);

	// scrcpy
	maxSize = jsonSetting["Config"].value("MaxSize", maxSize);
	maxFPS = jsonSetting["Config"].value("MaxFPS", maxFPS);
	bitrate = jsonSetting["Config"].value("Bitrate", bitrate);
	turnScreenOff = jsonSetting["Config"].value("TurnScreenOff", turnScreenOff);
	delayFrameCount = jsonSetting["Config"].value("DelayFrameCount", delayFrameCount);
	videoBuffer_ms = jsonSetting["Config"].value("VideoBuffer_ms", videoBuffer_ms);
	noResize = jsonSetting["Config"].value("NoResize", noResize);

	// sndcpy
	bufferMultiple = jsonSetting["Config"].value("BufferMultiple", bufferMultiple);
	maxBufferSampleCount = jsonSetting["Config"].value("MaxBufferSampleCount", maxBufferSampleCount);
	deviceMuteOnStart = jsonSetting["Config"].value("DeviceMuteOnStart", deviceMuteOnStart);
	toggleMuteReverse = jsonSetting["Config"].value("ToggleMuteReverse", toggleMuteReverse);

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

	std::stringstream ss;
	for (const char& c : loginPassword) {
		ss << "\\" << std::setfill('0') << std::setw(2) << std::hex << (c ^ obfuscation);
		//obfuPass.push_back(c ^ obfuscation);
	}
	std::string obfuPass = ss.str();
	jsonSetting["Config"]["LoginPassword"] = obfuPass;
	jsonSetting["Config"]["AutoWifiConnect"] = autoWifiConnect;
	jsonSetting["Config"]["ReconnectOnResume"] = reconnectOnResume;
	jsonSetting["Config"]["EnableUHID"] = enableUHID;
	jsonSetting["Config"]["UseScrcpyAudio"] = useScrcpyAudio;

	jsonSetting["Config"]["MaxSize"] = maxSize;
	jsonSetting["Config"]["MaxFPS"] = maxFPS;
	jsonSetting["Config"]["Bitrate"] = bitrate;
	jsonSetting["Config"]["TurnScreenOff"] = turnScreenOff;
	jsonSetting["Config"]["DelayFrameCount"] = delayFrameCount;
	jsonSetting["Config"]["VideoBuffer_ms"] = videoBuffer_ms;
	jsonSetting["Config"]["NoResize"] = noResize;

	jsonSetting["Config"]["BufferMultiple"] = bufferMultiple;
	jsonSetting["Config"]["MaxBufferSampleCount"] = maxBufferSampleCount;
	jsonSetting["Config"]["DeviceMuteOnStart"] = deviceMuteOnStart;
	jsonSetting["Config"]["ToggleMuteReverse"] = toggleMuteReverse;

	std::ofstream ofs((GetExeDirectory() / "setting.json").wstring());
	ofs << jsonSetting.dump(4);
}
