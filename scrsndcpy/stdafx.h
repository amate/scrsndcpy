// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//  are changed infrequently
//

#pragma once

// Change these values to use different versions
//#define WINVER		0x0601
//#define _WIN32_WINNT	0x0601
//#define _WIN32_IE	0x0700
#define _RICHEDIT_VER	0x0500

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlwin.h>
#include <atlstr.h>

#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif


#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlddx.h>

#include <vector>
#include <list>
#include <memory>
#include <string>
#include <ctime>
#include <random>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include <atomic>

#include <boost\filesystem.hpp>
#include <boost\circular_buffer.hpp>
#include <boost\optional.hpp>
#include <boost\algorithm\string\split.hpp>
#include <boost\algorithm\string\classification.hpp> // is_any_of
#include <boost\format.hpp>

#include "Utility\json.hpp"
#include "Utility\GdiplusUtil.h"
#include "Utility\CommonUtility.h"
#include "Utility\CodeConvert.h"
#include "Utility\Logger.h"
#include "Utility\timer.h"
