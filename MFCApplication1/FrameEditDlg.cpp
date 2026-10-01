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

BEGIN_MESSAGE_MAP(CFrameListHeader, CHeaderCtrl)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

// Feature 013: the header notices (HDN_ITEMDBLCLICK / HDN_DIVIDERDBLCLICK) go to
// the list, not to the editor window, and none is sent right of the last
// column, so the double-click itself is taken here. The default handling is
// skipped (no column auto-size). The request is posted so that the modal entry
// dialog is not opened inside the header's own message handling.
void CFrameListHeader::OnLButtonDblClk(UINT /*nFlags*/, CPoint /*point*/)
{
	CWnd* list = GetParent();
	CWnd* editor = list != nullptr ? list->GetParent() : nullptr;
	if (editor != nullptr && ::IsWindow(editor->GetSafeHwnd()))
		::PostMessage(editor->GetSafeHwnd(), WM_APP_FRAME_LIST_ADD, 0, 0);
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
	ON_MESSAGE(WM_APP_FRAME_LIST_ADD, &CFrameEditDlg::OnListAddRequest)
	ON_EN_CHANGE(IDC_EDIT_ECU_LABEL, &CFrameEditDlg::OnEcuLabelChanged)
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
	m_header.SubclassWindow(m_list.GetHeaderCtrl()->GetSafeHwnd()); // 013 FR-001

	RefreshList(-1);
	// 022: ECU label of the selected ECU
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_ECU_LABEL))->SetLimitText(CanSim::kMaxEcuLabelChars);
	ShowEcuLabel();
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
	GetDlgItem(IDC_BUTTON_FRAME_ADD)->EnableWindow(CanAdd());
	GetDlgItem(IDC_BUTTON_FRAME_EDIT)->EnableWindow(hasNode && selected);
	GetDlgItem(IDC_BUTTON_FRAME_DELETE)->EnableWindow(hasNode && selected);
}

bool CFrameEditDlg::CanAdd() const
{
	return SelectedNodeId() > 0 && m_list.GetItemCount() < CanSim::kMaxFramesPerNode;
}

// Feature 013: double-click in the frame list = Add (FR-001), only when the
// Add button would be enabled; otherwise nothing, without a message (FR-002).
void CFrameEditDlg::AddFromList()
{
	if (!CanAdd())
		return;
	OnAdd();
}

void CFrameEditDlg::OnNodeChanged()
{
	RefreshList(-1);
	ShowEcuLabel(); // 022
}

// 022: the label of the selected ECU (empty when it has none or when there is
// no ECU). Setting the text raises EN_CHANGE, which must not write it back.
void CFrameEditDlg::ShowEcuLabel()
{
	const CanSim::Node* node = CanSim::FindNodeDefinition(m_definitions, SelectedNodeId());
	m_settingLabel = true;
	SetDlgItemText(IDC_EDIT_ECU_LABEL, node != nullptr ? FromUtf8(node->Label) : CString());
	m_settingLabel = false;
	GetDlgItem(IDC_EDIT_ECU_LABEL)->EnableWindow(SelectedNodeId() > 0);
}

// 022 FR-002: every change is applied to the project at once and the main
// dialog redraws the diagram.
void CFrameEditDlg::OnEcuLabelChanged()
{
	if (m_settingLabel)
		return;
	const int nodeId = SelectedNodeId();
	if (nodeId <= 0)
		return;
	CString text;
	GetDlgItemText(IDC_EDIT_ECU_LABEL, text);
	const std::string utf8(CW2A(text, CP_UTF8));
	CanSim::EnsureNode(m_definitions, nodeId).Label = CanSim::LimitEcuLabel(CanSim::TrimEcuLabel(utf8));
	if (m_owner != nullptr && ::IsWindow(m_owner))
		::SendMessage(m_owner, WM_APP_ECU_LABEL_CHANGED, 0, 0);
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

// Feature 013: on a row or on the empty part alike, a double-click adds a frame
// (replaces "double-click = Edit" of feature 006; Edit is the button only).
// The scroll bars are non-client, so they do not send NM_DBLCLK (FR-004).
void CFrameEditDlg::OnListDoubleClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	AddFromList();
	*pResult = 0;
}

LRESULT CFrameEditDlg::OnListAddRequest(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	AddFromList();
	return 0;
}

void CFrameEditDlg::OnListItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	UpdateButtons();
	*pResult = 0;
}
