// SettingsDlg.h
// "Edit > Settings" dialog (feature 004 US1): ECU count, bit rate and
// simulation duration. UI strings live in MFCApplication1.rc. The speed is not
// here any more: it is set with the toolbar slider (feature 015).

#pragma once

#include "CanSim/CanTypes.h"

class CSettingsDlg : public CDialogEx
{
public:
	CSettingsDlg(const CanSim::ProjectSettings& settings, long long bitRateBps, CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SETTINGS };
#endif

	// Valid only after DoModal() returned IDOK.
	CanSim::ProjectSettings Settings;
	long long BitRateBps;

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()
};
