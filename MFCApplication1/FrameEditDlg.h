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
	DECLARE_MESSAGE_MAP()

private:
	int SelectedNodeId() const;   // 0 when there is no ECU
	int SelectedIndex() const;    // -1 when no row is selected
	void RefreshList(int selectIndex);
	void UpdateButtons();

	CanSim::Network& m_definitions;
	int m_ecuCount;
	int m_initialEcu;
	HWND m_owner = nullptr; // kept for the notice after the window is gone
	CListCtrl m_list;
	CComboBox m_node;
};
