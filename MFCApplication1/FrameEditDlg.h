// FrameEditDlg.h
// "Edit > Data frame editor" window (feature 006, domain spec 3.1a): pick a
// node, see its transmit frame list, add / edit / delete frames. Changes are
// applied to the project's definitions right away.
//
// Feature 010: the window is modeless. The owner creates it with new +
// Create; every way of closing it destroys the window, and the object deletes
// itself after sending WM_APP_FRAME_EDITOR_CLOSED to the owner.

#pragma once

#include "CanSim/CanTypes.h"

// Sent to the owner when the editor window has been destroyed.
const UINT WM_APP_FRAME_EDITOR_CLOSED = WM_APP + 1;

// Feature 013: posted by the list header to the editor window when the header
// is double-clicked (used only inside the editor window).
const UINT WM_APP_FRAME_LIST_ADD = WM_APP + 2;

// Feature 022: sent to the owner when the ECU label was edited.
const UINT WM_APP_ECU_LABEL_CHANGED = WM_APP + 3;

// Feature 024: posted by the main dialog to itself to load the project given
// on the command line (numbered here with the other WM_APP messages).
const UINT WM_APP_LOAD_STARTUP_PROJECT = WM_APP + 4;

// Feature 025: sent to the owner when the log viewer has been destroyed.
const UINT WM_APP_LOG_VIEWER_CLOSED = WM_APP + 5;

// Feature 028: sent to the owner when an analysis window has been destroyed.
const UINT WM_APP_LATENCY_CLOSED = WM_APP + 6;
const UINT WM_APP_BUSLOAD_CLOSED = WM_APP + 7;

// Feature 029: sent to the owner when the analysis window has been destroyed.
const UINT WM_APP_TXWAIT_CLOSED = WM_APP + 8;
const UINT WM_APP_JITTER_CLOSED = WM_APP + 9;

// Feature 013: header of the frame list. A double-click anywhere on it asks
// the editor window to add a frame instead of the default handling (which
// would auto-size a column on a divider).
class CFrameListHeader : public CHeaderCtrl
{
protected:
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
};

class CFrameEditDlg : public CDialogEx
{
public:
	CFrameEditDlg(CanSim::Network& definitions, int ecuCount, int initialEcu = 1, CWnd* pParent = nullptr);

	// ECU shown in the node list (1..16), 0 when there is no ECU.
	int CurrentEcu() const;

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FRAME_EDIT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	virtual void PostNcDestroy();
	afx_msg void OnNodeChanged();
	afx_msg void OnAdd();
	afx_msg void OnEdit();
	afx_msg void OnDelete();
	afx_msg void OnListDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnListAddRequest(WPARAM wParam, LPARAM lParam);
	afx_msg void OnEcuLabelChanged(); // feature 022
	DECLARE_MESSAGE_MAP()

private:
	int SelectedNodeId() const;   // 0 when there is no ECU
	int SelectedIndex() const;    // -1 when no row is selected
	void RefreshList(int selectIndex);
	void UpdateButtons();
	bool CanAdd() const;          // same condition as the Add button
	void AddFromList();           // double-click in the frame list (feature 013)
	void ShowEcuLabel();          // feature 022

	CanSim::Network& m_definitions;
	int m_ecuCount;
	int m_initialEcu;
	HWND m_owner = nullptr; // kept for the notice after the window is gone
	bool m_settingLabel = false; // 022: the label edit is being filled by the program
	CListCtrl m_list;
	CFrameListHeader m_header;
	CComboBox m_node;
};
