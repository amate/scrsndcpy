#pragma once

#include "Config.h"
//#include "DarkModeUI.h"
#include "resource.h"


class ConfigDlg : 
	public CDialogImpl<ConfigDlg>,
	public CWinDataExchange<ConfigDlg>
//	public DarkModeUI<ConfigDlg>
{
public:
	enum { IDD = IDD_CONFIG };

	ConfigDlg(Config& config);

	BEGIN_DDX_MAP(ConfigDlg)
		DDX_CHECK(IDC_CHECK_AUTO_START, m_tempConfig.autoStart)
		DDX_TEXT(IDC_EDIT_LOGIN_PASSWORD, m_loginPassword)
		DDX_CHECK(IDC_CHECK_AUTO_WIFI_CONNECT, m_tempConfig.autoWifiConnect)

		DDX_INT_RANGE(IDC_EDIT_MAX_SIZE, m_tempConfig.maxSize, 0, 10000)
		DDX_INT_RANGE(IDC_EDIT_MAX_FPS, m_tempConfig.maxFPS, 0, 120)
		DDX_INT_RANGE(IDC_EDIT_BIT_RATE, m_tempConfig.bitrate, 0, 50)
		DDX_INT_RANGE(IDC_EDIT_DELAY_FRAME_COUNT, m_tempConfig.delayFrameCount, 0u, 100u)		
		DDX_CHECK(IDC_CHECK_TURN_SCREEN_OFF, m_tempConfig.turnScreenOff)

		DDX_INT_RANGE(IDC_EDIT_BUFFER_MULTIPLE, m_tempConfig.bufferMultiple, 1, 10)
		DDX_INT_RANGE(IDC_EDIT_MAXBUFFERSAMPLECOUNT,m_tempConfig.maxBufferSampleCount, 0, 48000)		
		DDX_CHECK(IDC_CHECK_DEVICE_MUTE_ON_START, m_tempConfig.deviceMuteOnStart)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(ConfigDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)

	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);


private:
	Config&		m_config;

	CString		m_loginPassword;
	Config		m_tempConfig;

};
