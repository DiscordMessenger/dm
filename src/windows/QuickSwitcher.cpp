#include "QuickSwitcher.hpp"
#include "Main.hpp"

enum {
	COL_CHANNEL_NAME,
	COL_CATEGORY_NAME,
	COL_GUILD_NAME,
};

enum {
	ICN_CHANNEL,
	ICN_GROUPDM,
	ICN_VOICE,
	ICN_DM,
	ICN_GUILD,
	ICN_MAX,
};

static int g_qsImlIdxs[ICN_MAX];
static HIMAGELIST g_qsIml;

static int GetIconFromChannel(Channel* pChan)
{
	switch (pChan->m_channelType)
	{
		case Channel::DM:      return ICN_DM;
		case Channel::GROUPDM: return ICN_GROUPDM;
		case Channel::VOICE:   return ICN_VOICE;
		default:               return ICN_CHANNEL;
	}
}

struct QuickSwitchItem
{
	QuickMatch m_match;
	int m_iconIdx;
	Snowflake m_guildID;
	std::string m_name;
	std::string m_categ;
	std::string m_guild;

	QuickSwitchItem(QuickMatch& qm, int hi, const std::string& nm, const std::string& ct, const std::string& gl, Snowflake gi):
		m_match(qm), m_iconIdx(hi), m_name(nm), m_categ(ct), m_guild(gl), m_guildID(gi) {};
};

static std::vector<QuickSwitchItem> g_qsItems;

void QuickSwitcher::ShowDialog()
{
	g_qsItems.clear();
	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_QUICK_SWITCHER), g_Hwnd, &QuickSwitcher::DialogProc);
	g_qsItems.clear();
}

LRESULT QuickSwitcher::TextWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC oldWndProc = (WNDPROC) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_DESTROY:
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) oldWndProc);
			break;
			
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			HWND hList = GetDlgItem(GetParent(hWnd), IDC_CHANNEL_LIST);
			SendMessage(hList, uMsg, wParam, lParam);
			break;
		}
	}

	return CallWindowProc(oldWndProc, hWnd, uMsg, wParam, lParam);
}

void QuickSwitcher::OnUpdateQuery(HWND hWnd, const std::string& query)
{
	std::vector<QuickMatch> foundChannels = GetDiscordInstance()->Search(query);

	if (query.empty())
		SetDlgItemText(hWnd, IDC_QUICK_GROUP, TEXT("Previous Channels"));
	else switch (query[0])
	{
		case '@': SetDlgItemText(hWnd, IDC_QUICK_GROUP, TEXT("Searching Direct Messages")); break;
		case '!': SetDlgItemText(hWnd, IDC_QUICK_GROUP, TEXT("Searching Voice Channels")); break;
		case '#': SetDlgItemText(hWnd, IDC_QUICK_GROUP, TEXT("Searching Text Channels")); break;
		case '*': SetDlgItemText(hWnd, IDC_QUICK_GROUP, TEXT("Searching Servers")); break;
		default:  SetDlgItemText(hWnd, IDC_QUICK_GROUP, TEXT("Search Results")); break;
	}

	HWND hList = GetDlgItem(hWnd, IDC_CHANNEL_LIST);
	ListView_DeleteAllItems(hList);
	g_qsItems.clear();

	for (auto& match : foundChannels)
	{
		std::string name = "";
		std::string guildName = "";
		std::string categName = "";

		int iconIdx = ICN_GUILD;
		Snowflake guildID = 0;
		
		if (match.IsChannel())
		{
			Channel* pChan = GetDiscordInstance()->GetChannel(match.Id());

			if (!pChan) {
				DbgPrintW("Error, can't find channel %lld which DiscordInstance returned?", match.Id());
				continue;
			}

			iconIdx = GetIconFromChannel(pChan);
			name = pChan->m_name;

			Guild* pGuild = GetDiscordInstance()->GetGuild(pChan->m_parentGuild);
			if (!pGuild) {
				DbgPrintW("Error, can't find guild %lld for channel %lld (%s)", pChan->m_parentGuild, pChan->m_snowflake, pChan->m_name.c_str());
				continue;
			}

			guildName = pGuild->m_name;
			guildID = pChan->m_parentGuild;

			Channel* pCateg = pGuild->GetChannel(pChan->m_parentCateg);
			if (pCateg && pCateg != pChan)
				categName = pCateg->m_name;
		}
		else if (match.IsGuild())
		{
			Guild* pGuild = GetDiscordInstance()->GetGuild(match.Id());
			
			if (!pGuild) {
				DbgPrintW("Error, can't find guild %lld which DiscordInstance returned?", match.Id());
				continue;
			}

			name = pGuild->m_name;
			guildID = match.Id();
		}

		int idx = int(g_qsItems.size());
		g_qsItems.push_back(QuickSwitchItem(match, iconIdx, name, categName, guildName, guildID));

		LVITEM lv{};
		lv.mask = LVIF_TEXT | LVIF_IMAGE;
		lv.iItem = idx;
		lv.iImage = g_qsImlIdxs[iconIdx];
		lv.pszText = LPSTR_TEXTCALLBACK;
		ListView_InsertItem(hList, &lv);
	}

	//SetFocus(hList);
	ListView_SetItemState(hList, 0, LVIS_SELECTED, LVIS_SELECTED);
	ListView_EnsureVisible(hList, 0, TRUE);
}

void QuickSwitcher::SwitchToChannelAtIndex(int idx)
{
	if (idx < 0 || idx >= int(g_qsItems.size())) {
		DbgPrintW("Quick switch error, item is out of the bounds of what we have");
		return;
	}

	QuickSwitchItem& qsi = g_qsItems[idx];

	if (qsi.m_match.IsChannel())
		GetDiscordInstance()->OnSelectGuild(qsi.m_guildID, qsi.m_match.Id());
	else
		GetDiscordInstance()->OnSelectGuild(qsi.m_guildID);
}

void QuickSwitcher::SwitchToSelectedChannel(HWND hWnd)
{
	int iPos = ListView_GetNextItem(GetDlgItem(hWnd, IDC_CHANNEL_LIST), -1, LVNI_SELECTED);

	if (iPos == -1) {
		// TODO: disable the go button unless some item is selected
		DbgPrintW("Quick switch error, user didn't select anything, so can't go");
		return;
	}

	SwitchToChannelAtIndex(iPos);
}

void QuickSwitcher::HandleGetDispInfo(NMLVDISPINFO* pInfo)
{
	if (pInfo->item.iItem < 0 || pInfo->item.iItem >= int(g_qsItems.size()))
		return;

	QuickSwitchItem& item = g_qsItems[pInfo->item.iItem];

	static TCHAR buffer1[4096];
	static TCHAR buffer2[4096];
	static TCHAR buffer3[4096];
	switch (pInfo->item.iSubItem)
	{
		case COL_CHANNEL_NAME: {
			ConvertCppStringToTCharArray(item.m_name, buffer1, _countof(buffer1));
			pInfo->item.pszText = buffer1;
			break;
		}
		case COL_CATEGORY_NAME: {
			ConvertCppStringToTCharArray(item.m_categ, buffer2, _countof(buffer2));
			pInfo->item.pszText = buffer2;
			break;
		}
		case COL_GUILD_NAME: {
			ConvertCppStringToTCharArray(item.m_guild, buffer3, _countof(buffer3));
			pInfo->item.pszText = buffer3;
			break;
		}
	}
}

void QuickSwitcher::HandleItemChanged(HWND hWnd, NMLISTVIEW* pInfo)
{
	if (pInfo->iItem < 0 || pInfo->iItem >= int(g_qsItems.size()))
		return;

	if (~pInfo->uChanged & LVIF_STATE)
		return;

	// NOTE: We depend on the "unselect old item" notification to come before the "seleect new item" one.
	EnableWindow(GetDlgItem(hWnd, IDOK), !!(pInfo->uNewState & LVIS_SELECTED));
}

void QuickSwitcher::HandleItemActivate(HWND hWnd, NMITEMACTIVATE* pInfo)
{
	if (pInfo->iItem < 0 || pInfo->iItem >= int(g_qsItems.size()))
		return;

	SwitchToChannelAtIndex(pInfo->iItem);
	EndDialog(hWnd, IDOK);
}

LRESULT QuickSwitcher::HandleCustomDraw(HWND hWnd, NMLVCUSTOMDRAW* pInfo)
{
	switch (pInfo->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			return CDRF_NOTIFYSUBITEMDRAW;

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
			HWND hList = GetDlgItem(hWnd, IDC_CHANNEL_LIST);

			int idx = pInfo->nmcd.dwItemSpec;
			int sta = ListView_GetItemState(hList, idx, LVIS_SELECTED);

			if (sta & LVIS_SELECTED) {
				pInfo->clrTextBk = GetSysColor(COLOR_HIGHLIGHT);
				pInfo->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
			}
			else if (pInfo->iSubItem == COL_CATEGORY_NAME) {
				pInfo->clrTextBk = GetSysColor(COLOR_WINDOW);
				pInfo->clrText = GetSysColor(COLOR_GRAYTEXT);
			}
			else {
				pInfo->clrTextBk = GetSysColor(COLOR_WINDOW);
				pInfo->clrText = GetSysColor(COLOR_MENUTEXT);
			}

			return CDRF_NEWFONT;
		}

		default:DbgPrintW("Unhandled draw stage %d", pInfo->nmcd.dwDrawStage);
	}

	return CDRF_DODEFAULT;
}

void QuickSwitcher::CreateImageList()
{
	if (g_qsIml)
		DestroyImageList();

	int szIcon = GetSystemMetrics(SM_CXSMICON);
	g_qsIml = ImageList_Create(szIcon, szIcon, Supports32BitIcons() ? ILC_COLOR32 : ILC_COLOR24, ICN_MAX, 0);

	memset(g_qsImlIdxs, 0, sizeof g_qsImlIdxs);

	if (!g_qsIml) {
		DbgPrintW("Quick switcher: Cannot create image list");
		return;
	}

	g_qsImlIdxs[ICN_CHANNEL] = ImageList_AddIcon(g_qsIml, (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL)), IMAGE_ICON, szIcon, szIcon, LR_CREATEDIBSECTION | LR_SHARED));
	g_qsImlIdxs[ICN_VOICE]   = ImageList_AddIcon(g_qsIml, (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_VOICE)),   IMAGE_ICON, szIcon, szIcon, LR_CREATEDIBSECTION | LR_SHARED));
	g_qsImlIdxs[ICN_GROUPDM] = ImageList_AddIcon(g_qsIml, (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_GROUPDM)), IMAGE_ICON, szIcon, szIcon, LR_CREATEDIBSECTION | LR_SHARED));
	g_qsImlIdxs[ICN_DM]      = ImageList_AddIcon(g_qsIml, (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_DM)),      IMAGE_ICON, szIcon, szIcon, LR_CREATEDIBSECTION | LR_SHARED));
	g_qsImlIdxs[ICN_GUILD]   = ImageList_AddIcon(g_qsIml, (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_SERVER)),  IMAGE_ICON, szIcon, szIcon, LR_CREATEDIBSECTION | LR_SHARED));
}

void QuickSwitcher::DestroyImageList()
{
	if (!g_qsIml) return;

	ImageList_Destroy(g_qsIml);
	g_qsIml = NULL;
}

LRESULT QuickSwitcher::HijackWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC oldWndProc = (WNDPROC) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_DESTROY:
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) oldWndProc);
			break;
			
		case WM_NOTIFY:
			NMHDR* phdr = (NMHDR*)lParam;
			if (phdr->idFrom == IDC_CHANNEL_LIST && phdr->code == UINT(NM_CUSTOMDRAW))
				return HandleCustomDraw(hWnd, (NMLVCUSTOMDRAW*) lParam);

			break;
	}

	return CallWindowProc(oldWndProc, hWnd, uMsg, wParam, lParam);
}

INT_PTR QuickSwitcher::DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DESTROY:
			DestroyImageList();
			break;

		case WM_INITDIALOG:
		{
			CreateImageList();

			// Center on parent
			HWND hParent = GetParent(hWnd);

			RECT rcParent, rcChild;
			GetWindowRect(hParent, &rcParent);
			GetWindowRect(hWnd,    &rcChild);

			int x = rcParent.left + ((rcParent.right - rcParent.left) - (rcChild.right - rcChild.left)) / 2;
			int y = rcParent.top  + ((rcParent.bottom - rcParent.top) - (rcChild.bottom - rcChild.top)) / 2;
			int width  = rcChild.right - rcChild.left;
			int height = rcChild.bottom - rcChild.top;
			MoveWindow(hWnd, x, y, width, height, FALSE);

			// Initialize the list view
			HWND hList = GetDlgItem(hWnd, IDC_CHANNEL_LIST);

			RECT rect{};
			GetClientRect(hList, &rect);
			width = rect.right - rect.left - GetSystemMetrics(SM_CXVSCROLL);

			// Add columns
			int c1w, c2w;
			LVCOLUMN col{};
			col.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			col.pszText = TEXT("Channel");
			col.cx = c1w = MulDiv(width, 50, 100);
			col.iSubItem = 0;
			col.fmt = LVCFMT_LEFT;
			ListView_InsertColumn(hList, COL_CHANNEL_NAME, &col);

			col.pszText = TEXT("Category");
			col.cx = c2w = MulDiv(width, 20, 100);
			col.fmt = LVCFMT_RIGHT;
			ListView_InsertColumn(hList, COL_CATEGORY_NAME, &col);

			col.pszText = TEXT("Server");
			col.cx = width - c1w - c2w;
			col.fmt = LVCFMT_RIGHT;
			ListView_InsertColumn(hList, COL_GUILD_NAME, &col);

			ListView_SetImageList(hList, g_qsIml, LVSIL_NORMAL);
			ListView_SetImageList(hList, g_qsIml, LVSIL_SMALL);

			OnUpdateQuery(hWnd, "");

			// Go button is disabled by default, enabled when an item is selected
			EnableWindow(GetDlgItem(hWnd, IDOK), FALSE);

			ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);
			// Instantly focus on the searcher
			HWND hText = GetDlgItem(hWnd, IDC_QUICK_QUERY);
			SetFocus(hText);

			// Subclass the existing text box.  This allows input events to be forwarded to the list control.
			WNDPROC oldWndProc = (WNDPROC) GetWindowLongPtr(hText, GWLP_WNDPROC);
			SetWindowLongPtr(hText, GWLP_USERDATA, (LONG_PTR) oldWndProc);
			SetWindowLongPtr(hText, GWLP_WNDPROC,  (LONG_PTR) &QuickSwitcher::TextWndProc);

			// Hijack the window procedure of this dialog because I couldn't think of a less cluster-fucked
			// way to return custom draw return flags without the dialog default procedure intervening.
			oldWndProc = (WNDPROC) GetWindowLongPtr(hWnd, GWLP_WNDPROC);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) oldWndProc);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC,  (LONG_PTR) &QuickSwitcher::HijackWndProc);

			break;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wParam)) {
				case IDOK: {
					// Go!
					SwitchToSelectedChannel(hWnd);
					EndDialog(hWnd, IDOK);
					return TRUE;
				}
				case IDCANCEL: {
					// Cancel
					EndDialog(hWnd, IDCANCEL);
					return TRUE;
				}
				case IDC_QUICK_QUERY: {
					if (HIWORD(wParam) != EN_CHANGE)
						break;

					HWND hEdit = GetDlgItem(hWnd, IDC_QUICK_QUERY);
					int length = Edit_GetTextLength(hEdit);
					std::string query = "";
					if (length > 4096) {
						query = "\x1";
					}
					else if (length > 0) {
						TCHAR* tchr = new TCHAR[length + 1];
						Edit_GetText(hEdit, tchr, length + 1);
						query = MakeStringFromTString(tchr);
						delete[] tchr;
					}

					OnUpdateQuery(hWnd, query);
					break;
				}
			}
			break;
		}
		case WM_NOTIFY:
		{
			NMHDR* phdr = (NMHDR*) lParam;
			if (phdr->idFrom != IDC_CHANNEL_LIST)
				break;

			switch (phdr->code)
			{
				case LVN_GETDISPINFO:
					HandleGetDispInfo((NMLVDISPINFO*) lParam);
					break;

				case LVN_ITEMCHANGED:
					HandleItemChanged(hWnd, (NMLISTVIEW*) lParam);
					break;

				case NM_DBLCLK:
					HandleItemActivate(hWnd, (NMITEMACTIVATE*) lParam);
					break;
			}

			break;
		}
	}

	return FALSE;
}
