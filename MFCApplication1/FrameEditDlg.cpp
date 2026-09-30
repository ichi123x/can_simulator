// FrameEditDlg.cpp
#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "FrameEditDlg.h"
#include "FrameEntryDlg.h"
#include "afxdialogex.h"

#include "CanSim/FrameEditor.h"

namespace
{
	CString LoadRc(UINT id)
	{
		CString s;
		s.LoadString(id);
		return s;
	}

	CString FormatMs(long long us)
	{
		CString s;
		if (us % 1000 == 0)
			s.Format(_T("%I64d"), us / 1000);
		else
			s.Format(_T("%.3f"), static_cast<double>(us) / 1000.0);
		return s;
	}

	CString FromUtf8(const std::string& text)
	{
		return CString(CA2W(text.c_str(), CP_UTF8));
	}

	// Keeps the owner (main dialog) disabled while a nested modal dialog is
	// shown, so that it cannot close or replace this window meanwhile
	// (feature 010, research.md decision 8).
	class OwnerDisabler
	{
	public:
		explicit OwnerDisabler(HWND owner)
			: m_owner(owner != nullptr && ::IsWindowEnabled(owner) ? owner : nullptr)
		{
			if (m_owner != nullptr)
				::EnableWindow(m_owner, FALSE);
		}
		~OwnerDisabler()
		{
			if (m_owner != nullptr)
				::EnableWindow(m_owner, TRUE);
		}
		OwnerDisabler(const OwnerDisabler&) = delete;
		OwnerDisabler& operator=(const OwnerDisabler&) = delete;

	private:
		HWND m_owner;
	};
}

CFrameEditDlg::CFrameEditDlg(CanSim::Network& definitions, int ecuCount, int initialEcu, CWnd* pParent)
	: CDialogEx(IDD_FRAME_EDIT, pParent)
	, m_definitions(definitions)
	, m_ecuCount(ecuCount)
	, m_initialEcu(initialEcu)
{
}

void CFrameEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_FRAMES, m_list);
	DDX_Control(pDX, IDC_COMBO_FRAME_NODE, m_node);
}

BEGIN_MESSAGE_MAP(CFrameEditDlg, CDialogEx)
	ON_CBN_SELCHANGE(IDC_COMBO_FRAME_NODE, &CFrameEditDlg::OnNodeChanged)
	ON_BN_CLICKED(IDC_BUTTON_FRAME_ADD, &CFrameEditDlg::OnAdd)
	ON_BN_CLICKED(IDC_BUTTON_FRAME_EDIT, &CFrameEditDlg::OnEdit)
	ON_BN_CLICKED(IDC_BUTTON_FRAME_DELETE, &CFrameEditDlg::OnDelete)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_FRAMES, &CFrameEditDlg::OnListDoubleClick)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_FRAMES, &CFrameEditDlg::OnListItemChanged)
END_MESSAGE_MAP()

BOOL CFrameEditDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_owner = ::GetWindow(m_hWnd, GW_OWNER);

	// Nodes: ECU 01 .. ECU NN within the current ECU count (006 FR-002)
	for (int n = 1; n <= m_ecuCount; ++n)
	{
		CString text;
		text.Format(_T("ECU %02d"), n);
		m_node.AddString(text);
	}
	if (m_ecuCount > 0)
		m_node.SetCurSel((std::min)((std::max)(m_initialEcu, 1), m_ecuCount) - 1); // 010 FR-002

	// Columns in the order of domain spec FR-086a (006 FR-003)
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	const struct { UINT Title; int Width; int Format; } columns[] = {
		{ IDS_FRAME_COL_NO, 40, LVCFMT_RIGHT },
		{ IDS_FRAME_COL_NAME, 140, LVCFMT_LEFT },
		{ IDS_FRAME_COL_ID, 60, LVCFMT_LEFT },
		{ IDS_FRAME_COL_DLC, 40, LVCFMT_RIGHT },
		{ IDS_FRAME_COL_OFFSET, 110, LVCFMT_RIGHT },
		{ IDS_FRAME_COL_PERIOD, 100, LVCFMT_RIGHT },
	};
	CClientDC dc(this);
	const int dpi = dc.GetDeviceCaps(LOGPIXELSX);
	for (int i = 0; i < _countof(columns); ++i)
		m_list.InsertColumn(i, LoadRc(columns[i].Title), columns[i].Format, MulDiv(columns[i].Width, dpi, 96));

	RefreshList(-1);
	return TRUE;
}

// Feature 010: modeless. Enter, Esc, "Close" and the close box all destroy the
// window; the edits are already in the project's definitions (FR-016).
void CFrameEditDlg::OnOK()
{
	DestroyWindow();
}

void CFrameEditDlg::OnCancel()
{
	DestroyWindow();
}

void CFrameEditDlg::PostNcDestroy()
{
	CDialogEx::PostNcDestroy();
	if (m_owner != nullptr && ::IsWindow(m_owner))
		::SendMessage(m_owner, WM_APP_FRAME_EDITOR_CLOSED, 0, 0);
	delete this;
}

int CFrameEditDlg::CurrentEcu() const
{
	return SelectedNodeId();
}

int CFrameEditDlg::SelectedNodeId() const
{
	const int sel = m_node.GetCurSel();
	return sel >= 0 ? sel + 1 : 0;
}

int CFrameEditDlg::SelectedIndex() const
{
	POSITION pos = m_list.GetFirstSelectedItemPosition();
	return pos != nullptr ? m_list.GetNextSelectedItem(pos) : -1;
}

void CFrameEditDlg::RefreshList(int selectIndex)
{
	m_list.DeleteAllItems();
	const CanSim::Node* node = CanSim::FindNodeDefinition(m_definitions, SelectedNodeId());
	if (node != nullptr)
	{
		for (std::size_t i = 0; i < node->Frames.size(); ++i)
		{
			const CanSim::FrameDefinition& f = node->Frames[i];
			CString text;
			text.Format(_T("%d"), static_cast<int>(i) + 1); // No. = position (006 FR-009)
			const int row = m_list.InsertItem(static_cast<int>(i), text);
			m_list.SetItemText(row, 1, FromUtf8(f.Name));
			text.Format(_T("0x%03X"), f.CanId);
			m_list.SetItemText(row, 2, text);
			text.Format(_T("%u"), f.Dlc);
			m_list.SetItemText(row, 3, text);
			m_list.SetItemText(row, 4, FormatMs(f.InitialOffsetUs));
			m_list.SetItemText(row, 5, FormatMs(f.PeriodUs));
		}
	}
	if (selectIndex >= 0 && selectIndex < m_list.GetItemCount())
	{
		m_list.SetItemState(selectIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_list.EnsureVisible(selectIndex, FALSE);
	}
	UpdateButtons();
}

// 006 FR-004: Edit / Delete need a selected row; Add is off at 100 frames or
// when there is no ECU.
void CFrameEditDlg::UpdateButtons()
{
	const bool hasNode = SelectedNodeId() > 0;
	const bool selected = SelectedIndex() >= 0;
	GetDlgItem(IDC_BUTTON_FRAME_ADD)->EnableWindow(hasNode && m_list.GetItemCount() < CanSim::kMaxFramesPerNode);
	GetDlgItem(IDC_BUTTON_FRAME_EDIT)->EnableWindow(hasNode && selected);
	GetDlgItem(IDC_BUTTON_FRAME_DELETE)->EnableWindow(hasNode && selected);
}

void CFrameEditDlg::OnNodeChanged()
{
	RefreshList(-1);
}

void CFrameEditDlg::OnAdd()
{
	const int nodeId = SelectedNodeId();
	if (nodeId <= 0)
		return;
	CFrameEntryDlg dlg(m_definitions, nodeId, -1, m_definitions.BitRateBps, this);
	{
		OwnerDisabler lock(m_owner);
		if (dlg.DoModal() != IDOK)
			return;
	}
	CanSim::AddFrame(m_definitions, nodeId, dlg.Frame);
	const CanSim::Node* node = CanSim::FindNodeDefinition(m_definitions, nodeId);
	RefreshList(node != nullptr ? static_cast<int>(node->Frames.size()) - 1 : -1);
}

void CFrameEditDlg::OnEdit()
{
	const int nodeId = SelectedNodeId();
	const int index = SelectedIndex();
	if (nodeId <= 0 || index < 0)
		return;
	CFrameEntryDlg dlg(m_definitions, nodeId, index, m_definitions.BitRateBps, this);
	{
		OwnerDisabler lock(m_owner);
		if (dlg.DoModal() != IDOK)
			return;
	}
	CanSim::ReplaceFrame(m_definitions, nodeId, index, dlg.Frame);
	RefreshList(index);
}

void CFrameEditDlg::OnDelete()
{
	const int nodeId = SelectedNodeId();
	const int index = SelectedIndex();
	const CanSim::Node* node = CanSim::FindNodeDefinition(m_definitions, nodeId);
	if (node == nullptr || index < 0 || index >= static_cast<int>(node->Frames.size()))
		return;

	// 006 FR-008: confirm with No., name and ID first.
	const CanSim::FrameDefinition& f = node->Frames[static_cast<std::size_t>(index)];
	CString message;
	message.Format(LoadRc(IDS_FRAME_DELETE_CONFIRM), index + 1, (LPCTSTR)FromUtf8(f.Name), f.CanId);
	{
		OwnerDisabler lock(m_owner);
		if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) != IDYES)
			return;
	}

	CanSim::DeleteFrame(m_definitions, nodeId, index);
	RefreshList(-1);
}

void CFrameEditDlg::OnListDoubleClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	OnEdit();
	*pResult = 0;
}

void CFrameEditDlg::OnListItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	UpdateButtons();
	*pResult = 0;
}
