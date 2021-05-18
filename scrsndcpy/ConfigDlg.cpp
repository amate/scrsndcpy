#include "stdafx.h"
#include "ConfigDlg.h"

#include "Utility\json.hpp"
#include "Utility\CommonUtility.h"
#include "Utility\CodeConvert.h"
#include "Utility\Logger.h"

using json = nlohmann::json;
using namespace CodeConvert;

ConfigDlg::ConfigDlg(Config& config) : m_config(config)
{
}

LRESULT ConfigDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	m_tempConfig = m_config;

	m_loginPassword = UTF16fromUTF8(m_tempConfig.loginPassword).c_str();

	DoDataExchange(DDX_LOAD);
	return 0;
}

LRESULT ConfigDlg::OnOK(WORD, WORD wID, HWND, BOOL&)
{
	if (!DoDataExchange(DDX_SAVE)) {
		MessageBox(L"数値を範囲内に収めてください");
		return 0;
	}

	m_tempConfig.loginPassword = UTF8fromUTF16((LPCWSTR)m_loginPassword);

	m_config = m_tempConfig;
	m_config.SaveConfig();

	EndDialog(IDOK);
	return 0;
}

LRESULT ConfigDlg::OnCancel(WORD, WORD, HWND, BOOL&)
{
	EndDialog(IDCANCEL);
	return 0;
}
