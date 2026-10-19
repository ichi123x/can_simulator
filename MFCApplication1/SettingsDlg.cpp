// SettingsDlg.cpp
#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "SettingsDlg.h"
#include "afxdialogex.h"

namespace
{
	struct BitRateChoice
	{
		long long Bps;
		const TCHAR* Label;
	};

	const BitRateChoice kBitRates[] = {
		{ 125000, _T("125 kbps") },
		{ 250000, _T("250 kbps") },
		{ 500000, _T("500 kbps") },
		{ 1000000, _T("1 Mbps") },
	};
}

CSettingsDlg::CSettingsDlg(const CanSim::ProjectSettings& settings, long long bitRateBps, CWnd* pParent)
	: CDialogEx(IDD_SETTINGS, pParent)
	, Settings(settings)
	, BitRateBps(bitRateBps)
{
}

BEGIN_MESSAGE_MAP(CSettingsDlg, CDialogEx)
END_MESSAGE_MAP()

BOOL CSettingsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ECU count: 0..16 only (004 FR-005)
	CComboBox* ecu = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_ECU_COUNT));
	for (int n = 0; n <= 16; ++n)
	{
		CString text;
		text.Format(_T("%d"), n);
		ecu->AddString(text);
	}
	ecu->SetCurSel(Settings.EcuCount);

	// Bit rate: the four values of 001 FR-002 only
	CComboBox* rate = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_BITRATE));
	int selected = 2; // 500 kbps
	for (int i = 0; i < _countof(kBitRates); ++i)
	{
		rate->AddString(kBitRates[i].Label);
		if (kBitRates[i].Bps == BitRateBps)
			selected = i;
	}
	rate->SetCurSel(selected);

	CString duration;
	duration.Format(_T("%d"), Settings.DurationSec);
	SetDlgItemText(IDC_EDIT_DURATION_SEC, duration);

	return TRUE;
}

void CSettingsDlg::OnOK()
{
	CString text;
	GetDlgItemText(IDC_EDIT_DURATION_SEC, text);
	text.Trim();
	bool valid = !text.IsEmpty() && text.GetLength() <= 4 && text.SpanIncluding(_T("0123456789")) == text;
	const int duration = valid ? _ttoi(text) : 0;
	valid = valid && duration >= CanSim::kMinDurationSec && duration <= CanSim::kMaxDurationSec;
	if (!valid)
	{
		CString message;
		VERIFY(message.LoadString(IDS_DURATION_ERROR));
		AfxMessageBox(message, MB_OK | MB_ICONWARNING);
		GetDlgItem(IDC_EDIT_DURATION_SEC)->SetFocus();
		return; // keep the dialog open
	}

	const int ecuCount = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_ECU_COUNT))->GetCurSel();
	const int rateIndex = static_cast<CComboBox*>(GetDlgItem(IDC_COMBO_BITRATE))->GetCurSel();
	Settings.EcuCount = ecuCount < 0 ? 0 : ecuCount;
	Settings.DurationSec = duration;
	if (rateIndex >= 0 && rateIndex < _countof(kBitRates))
		BitRateBps = kBitRates[rateIndex].Bps;
	// Settings.Speed is returned unchanged: the speed is set with the toolbar
	// slider (feature 015).

	CDialogEx::OnOK();
}
