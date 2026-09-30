// FrameEntryDlg.h
// Entry dialog used by "Add" and "Edit" of the data frame editor (feature
// 006 US2/US3). Validation is done by CanSim::ValidateFrame; this dialog only
// shows the issues and the computed bit length / transmission time.

#pragma once

#include "CanSim/CanTypes.h"

class CFrameEntryDlg : public CDialogEx
{
public:
	// editIndex = -1 adds a frame to nodeId; otherwise edits that frame.
	CFrameEntryDlg(const CanSim::Network& definitions, int nodeId, int editIndex, long long bitRateBps,
		CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FRAME_ENTRY };
#endif

	// Valid after DoModal() returned IDOK.
	CanSim::FrameDefinition Frame;

protected:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnIdOrDlcChanged();
	DECLARE_MESSAGE_MAP()

private:
	void UpdateBitLength();

	const CanSim::Network& m_definitions;
	int m_nodeId;
	int m_editIndex;
	long long m_bitRateBps;
};
