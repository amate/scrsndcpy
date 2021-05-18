// scrsndcpy.cpp : main source file for scrsndcpy.exe
//

#include "stdafx.h"

#include "MainDlg.h"
#include "Utility\CommonUtility.h"
#include "Utility\GdiplusUtil.h"
#include "Socket.h"

CAppModule _Module;


std::string	LogFileName()	// for Logger
{
	auto logPath = GetExeDirectory() / L"info.log";
	return logPath.string();
}

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainDlg dlgMain;

	if(dlgMain.Create(NULL) == NULL)
	{
		ATLTRACE(_T("Main dialog creation failed!\n"));
		return 0;
	}

	dlgMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	CSocket::Init();
	GdiplusInit();

	int nRet = Run(lpstrCmdLine, nCmdShow);

	GdiplusTerm();
	CSocket::Term();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
