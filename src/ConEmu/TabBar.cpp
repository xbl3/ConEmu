#include <windows.h>
#include <commctrl.h>
#include "header.h"

TabBarClass TabBar;
const int TAB_FONT_HEIGTH = 16;
wchar_t TAB_FONT_FACE[] = L"Tahoma";
WNDPROC _defaultTabProc = NULL;

TabBarClass::TabBarClass()
{
	_active = false;
	_hwndTab = NULL;
	_tabHeight = NULL; memset(&m_Margins, 0, sizeof(m_Margins));
	_titleShouldChange = false;
	_prevTab = -1;
	mb_ChangeAllowed = FALSE;
	mb_Enabled = TRUE;
	mh_ToolbarParent = NULL; mh_Toolbar = NULL;
	GetConsolesTitles = NULL;
	ActivateConsole = NULL;
}

void TabBarClass::Enable(BOOL abEnabled)
{
	if (_hwndTab && mb_Enabled!=abEnabled)
	{
		EnableWindow(_hwndTab, abEnabled);
		mb_Enabled = abEnabled;
	}
}

void TabBarClass::Refresh(BOOL abFarActive)
{
	Enable(abFarActive);
}

void TabBarClass::Reset()
{
	if (!_active)
	{
		return;
	}

	ConEmuTab tab; memset(&tab, 0, sizeof(tab));
	tab.Pos=0;
	tab.Current=1;
	tab.Type = 1;
	TabBar.Update(&tab, 1);
}

void TabBarClass::AddTab(wchar_t* text, int i)
{
	TCITEM tie;
	tie.mask = TCIF_TEXT;
	tie.iImage = -1; 
	tie.pszText = text ;

	int nCurCount = TabCtrl_GetItemCount(_hwndTab);
	if (i>=nCurCount)
		TabCtrl_InsertItem(_hwndTab, i, &tie);
	else
		TabCtrl_SetItem(_hwndTab, i, &tie);
}

void TabBarClass::SelectTab(int i)
{
    mb_ChangeAllowed = TRUE;
	TabCtrl_SetCurSel(_hwndTab, i);
	mb_ChangeAllowed = FALSE;
}

char TabBarClass::FarTabShortcut(int tabIndex)
{
	return tabIndex < 10 ? '0' + tabIndex : 'A' + tabIndex - 10;
}

void TabBarClass::FarSendChangeTab(int tabIndex)
{
	PostMessage(ghConWnd, WM_KEYDOWN, VK_F14, 0);
	PostMessage(ghConWnd, WM_KEYUP, VK_F14, 0);
	PostMessage(ghConWnd, WM_KEYDOWN, FarTabShortcut(tabIndex), 0);
	PostMessage(ghConWnd, WM_KEYUP, FarTabShortcut(tabIndex), 0);
}

static LRESULT CALLBACK TabProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		{
			TabBar.OnMouse(uMsg, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		}
	}
	return CallWindowProc(_defaultTabProc, hwnd, uMsg, wParam, lParam);
}


bool TabBarClass::IsActive()
{
	return _active;
}

bool TabBarClass::IsShown()
{
	return _active && IsWindowVisible(_hwndTab);
}

/*int TabBarClass::Height()
{
	return _tabHeight;
}*/

BOOL TabBarClass::IsAllowed()
{
	BOOL lbTabsAllowed = TRUE;
	if (gSet.BufferHeight) {
        CONSOLE_SCREEN_BUFFER_INFO inf; memset(&inf, 0, sizeof(inf));
        GetConsoleScreenBufferInfo(pVCon->hConOut(), &inf);
        if (inf.dwSize.Y>(inf.srWindow.Bottom-inf.srWindow.Top+1))
			lbTabsAllowed = FALSE;
	}
	return lbTabsAllowed;
}

void TabBarClass::Activate()
{
	if (!_hwndTab) {
		RECT rcClient; 
		GetClientRect(ghWnd, &rcClient); 
		InitCommonControls(); 
		DWORD nPlacement = TCS_SINGLELINE;
		_hwndTab = CreateWindow(WC_TABCONTROL, NULL, nPlacement | WS_CHILD | WS_CLIPSIBLINGS | TCS_FOCUSNEVER, 0, 0, 
			rcClient.right, 0, ghWnd, NULL, g_hInstance, NULL);
		if (_hwndTab == NULL)
		{ 
			return; 
		}
		HFONT hFont = CreateFont(TAB_FONT_HEIGTH, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, 
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, TAB_FONT_FACE);
		SendMessage(_hwndTab, WM_SETFONT, WPARAM (hFont), TRUE);
		
		#pragma warning (disable : 4312)
		_defaultTabProc = (WNDPROC)SetWindowLongPtr(_hwndTab, GWL_WNDPROC, (LONG_PTR)TabProc);

		#ifdef _DEBUG	
		CreateToolbar();
		#endif
	}

	_active = true;
}

void TabBarClass::Deactivate()
{
	if (!_active /*|| !_tabHeight*/)
		return;

	_tabHeight = 0; memset(&m_Margins, 0, sizeof(m_Margins));
	UpdatePosition();
	_active = false;
}

void TabBarClass::Update(ConEmuTab* tabs, int tabsCount)
{
	if (!_active)
	{
		return;
	}
	
	int i;
	//TabCtrl_DeleteAllItems(_hwndTab);
	for (i = 0; i < tabsCount; i++)
	{
		// get file name
		TCHAR dummy[MAX_PATH*2];
		TCHAR fileName[MAX_PATH+4];
		TCHAR szFormat[32];
		wchar_t* tFileName=NULL; //--Maximus
		if (tabs[i].Name[0]==0 || tabs[i].Type == 1/*WTYPE_PANELS*/) {
			_tcscpy(tabs[i].Name, gSet.szTabPanels);
			tFileName = tabs[i].Name;
			_tcscpy(szFormat, _T("%s"));
		} else {
			GetFullPathName(tabs[i].Name, MAX_PATH*2, dummy, &tFileName);
			if (!tFileName)
				tFileName = tabs[i].Name;

			if (tabs[i].Type == 3/*WTYPE_EDITOR*/) {
				if (tabs[i].Modified)
					_tcscpy(szFormat, gSet.szTabEditorModified);
				else
					_tcscpy(szFormat, gSet.szTabEditor);
			} 
			else if (tabs[i].Type == 2/*WTYPE_VIEWER*/)
				_tcscpy(szFormat, gSet.szTabViewer);
		}
		// restrict length
		int origLength = _tcslen(tFileName);
		int nMaxLen = gSet.nTabLenMax - _tcslen(szFormat) + 2/* %s */;
		if (nMaxLen<15) nMaxLen=15; else
			if (nMaxLen>=MAX_PATH) nMaxLen=MAX_PATH-1;
		if (origLength > nMaxLen)
		{
			/*_tcsnset(fileName, _T('\0'), MAX_PATH);
			_tcsncat(fileName, tFileName, 10);
			_tcsncat(fileName, _T("..."), 3);
			_tcsncat(fileName, tFileName + origLength - 10, 10);*/
			int nSplit = nMaxLen*2/3;
			TCHAR szEllip[MAX_PATH+1];
			_tcsncpy(szEllip, tFileName, nSplit); szEllip[nSplit]=0;
			_tcscat(szEllip, _T("..."));
			_tcscat(szEllip, tFileName + origLength - (nMaxLen - nSplit));
			wsprintf(fileName, szFormat, szEllip);
		} else {
			wsprintf(fileName, szFormat, tFileName);
		}

		AddTab(fileName, i);
	}
	
	// ������� ������ ��������
	int nCurCount = TabCtrl_GetItemCount(_hwndTab);
	for (i=tabsCount; i<nCurCount; i++)
		TabCtrl_DeleteItem(_hwndTab, i);

	if (tabsCount) {
		int ncur = 0;
		for (i = tabsCount-1; i >= 0; i--)
		{
			if (tabs[i].Current)
			{
				ncur = i; break;
			}
		}
		SelectTab(ncur);
	}
	
	if (_tabHeight && tabsCount==1 && tabs[0].Type == 1/*WTYPE_PANELS*/ && gSet.isTabs==2) {
		// ����������� ����� (��� ���������/������� �������)
		Deactivate();
	} else
	if (_tabHeight == NULL)
	{
		RECT rcClient, rcWnd;
		GetClientRect(ghWnd, &rcClient); 
		rcWnd = rcClient;
		TabCtrl_AdjustRect(_hwndTab, FALSE, &rcClient);
		_tabHeight = rcClient.top;

		m_Margins.top = rcClient.top;
		if (gSet.isTabFrame)
		{
			m_Margins.left = rcClient.left;
			m_Margins.right = rcWnd.right-rcClient.right;
			m_Margins.bottom = rcWnd.bottom-rcClient.bottom;
		}
		gSet.UpdateMargins(m_Margins);

		UpdatePosition();
	}
}

RECT TabBarClass::GetMargins()
{
	return m_Margins;
}

void TabBarClass::UpdatePosition()
{
	if (!_active)
	{
		return;
	}
	RECT client, self;
	GetClientRect(ghWnd, &client);
	GetWindowRect(_hwndTab, &self);
	
	gConEmu.ReSize();
	
	if (_tabHeight>0) {
		ShowWindow(_hwndTab, SW_SHOW);
		if (gSet.isTabFrame)
			MoveWindow(_hwndTab, 0, 0, client.right, client.bottom, 1);
		else
			MoveWindow(_hwndTab, 0, 0, client.right, _tabHeight, 1);
	} else {
		ShowWindow(_hwndTab, SW_HIDE);
	}
}

void TabBarClass::UpdateWidth()
{
	if (!_active)
	{
		return;
	}
	RECT client, self;
	GetClientRect(ghWnd, &client);
	GetWindowRect(_hwndTab, &self);
	if (gSet.isTabFrame) {
		MoveWindow(_hwndTab, 0, 0, client.right, client.bottom, 1);
	} else {
		MoveWindow(_hwndTab, 0, 0, client.right, _tabHeight, 1);
	}
	if (mh_ToolbarParent) {
		//SetWindowPos(mh_ToolbarParent, HWND_TOP, 0,0,0,0, SWP_NOSIZE|SWP_NOMOVE);
		   SIZE sz; 
		   SendMessage(mh_Toolbar, TB_GETMAXSIZE, 0, (LPARAM)&sz);
		   RECT rcClient;
		   GetWindowRect(_hwndTab, &rcClient);
		   MapWindowPoints(NULL, ghWnd, (LPPOINT)&rcClient, 2);
		   //int nbWidth = SendMessage(mh_Toolbar, TB_GETBUTTONSIZE, 0, 0) & 0xFFFF;
		   //int nHidden = 10;
		   SetWindowPos(mh_ToolbarParent, HWND_TOP, 
			   rcClient.right - sz.cx - 2, 0,
			   sz.cx, sz.cy, 0);
	}
}

bool TabBarClass::OnNotify(LPNMHDR nmhdr)
{
	if (!_active)
	{
		return false;
	}

	SetFocus(ghWnd);
	if (nmhdr->code == TCN_SELCHANGING)
	{
		//if (mb_ChangeAllowed) {
		//	return FALSE;
		//}
		_prevTab = TabCtrl_GetCurSel(_hwndTab); 
		return FALSE; // ���������
	}

	if (nmhdr->code == TCN_SELCHANGE)
	{
		int lnNewTab = TabCtrl_GetCurSel(_hwndTab);
		//_tcscpy(_lastTitle, gConEmu.Title);
		
		if (_prevTab>=0) {
			SelectTab(_prevTab);
			_prevTab = -1;
		}
		
		if (mb_ChangeAllowed) {
			return FALSE;
		}
		
		FarSendChangeTab(lnNewTab);
		// start waiting for title to change
		_titleShouldChange = true;
		return true;
	} 
	return false;
}



void TabBarClass::OnTimer()
{
	//if (!_active)
	//{
	//	return;
	//}
	//if (_titleShouldChange)
	//{
	//	// title hasn't changed - tab switching was unsuccessful - return to the previous selected tab
	//	if (_tcscmp(_lastTitle, gConEmu.Title) == 0)
	//	{
	//		SelectTab(_prevTab);
	//	}
	//	_titleShouldChange = false;
	//}
	return;
}

void TabBarClass::OnMouse(int message, int x, int y)
{
	if (!_active)
	{
		return;
	}

	//SetFocus(ghWnd);
	if (message == WM_MBUTTONUP || message == WM_RBUTTONUP)
	{
		TCHITTESTINFO htInfo;
		htInfo.pt.x = x;
		htInfo.pt.y = y;
		int iPage = TabCtrl_HitTest(_hwndTab, &htInfo);
		if (iPage != -1)
		{
			FarSendChangeTab(iPage);
			Sleep(50); // TODO
			PostMessage(ghConWnd, WM_KEYDOWN, VK_F10, 0);
			PostMessage(ghConWnd, WM_KEYUP, VK_F10, 0);
		}
	}
}

void TabBarClass::Invalidate()
{
	if (TabBar.IsActive())
		InvalidateRect(_hwndTab, NULL, TRUE);
}

LRESULT TabBarClass::ToolWndProc(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam)
{
	switch (messg) {
		case WM_COMMAND:
			{
				if (!gConEmu.isConman())
					break;
				if (!gConEmu.mh_Infis) {
					gConEmu.mh_Infis = LoadLibrary ( gConEmu.ms_InfisPath );
					if (gConEmu.mh_Infis == NULL) {
						gConEmu.mh_Infis = (HMODULE)INVALID_HANDLE_VALUE;
						DisplayLastError(_T("Can't load infis.dll! Old ConMan version?"));
					}
				}
				if (!gConEmu.mh_Infis || gConEmu.mh_Infis==INVALID_HANDLE_VALUE)
					break;

				if (!TabBar.GetConsolesTitles)
					TabBar.GetConsolesTitles =
						(GetConsolesTitles_t*)GetProcAddress( gConEmu.mh_Infis, "GetConsolesTitles" );
				if (!TabBar.ActivateConsole)
					TabBar.ActivateConsole =
						(ActivateConsole_t*)GetProcAddress( gConEmu.mh_Infis, "ActivateConsole" );
				if (!TabBar.GetConsolesTitles || !TabBar.ActivateConsole)
					break;

				switch (wParam)
				{
				case 1: case 2: case 3: case 4: case 5: case 6:
				case 7: case 8: case 9: case 10: case 11: case 12:
					// ������������ ������� �
					TabBar.ActivateConsole(wParam);
					break; 
				case 13:
					// ������� ����� �������
					break;
				case 14:
					// ������������ ����� �������������� ��������
					break;
				}
			}
			break;
	}
	return DefWindowProc(hWnd, messg, wParam, lParam);
}

void TabBarClass::CreateToolbar()
{
	if (mh_Toolbar) return;
	
	WNDCLASS wc = {CS_DBLCLKS, (WNDPROC)TabBarClass::ToolWndProc, 0, 0, 
		    g_hInstance, 0, LoadCursor(NULL, IDC_ARROW), 
		    (HBRUSH)(COLOR_BTNFACE+1), 
		    NULL, _T("VirtualConsoleClassConMan")};
    if (!RegisterClass(&wc))
        return;

	
	mh_ToolbarParent = CreateWindow(_T("VirtualConsoleClassConMan"), _T(""), 
			WS_VISIBLE|WS_CHILD, 300,0,340,22, ghWnd, 0, 0, 0);
	if (!mh_ToolbarParent) return;
	
   INITCOMMONCONTROLSEX icex;
    
// Ensure that the common control DLL is loaded. 
   icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
   icex.dwICC  = ICC_BAR_CLASSES;
   InitCommonControlsEx(&icex);
 
// Create a toolbar. 
	TBBUTTON buttons[16] = {
		{0, 1, TBSTATE_ENABLED, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{1, 2, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{2, 3, TBSTATE_ENABLED, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{3, 4, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{4, 5, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{5, 6, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{6, 7, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{7, 8, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{8, 9, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{9, 10, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{10, 11, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{11, 12, TBSTATE_ENABLED|TBSTATE_HIDDEN, TBSTYLE_CHECKGROUP/*|TBSTYLE_TRANSPARENT*/},
		{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP},
		{12, 13, TBSTATE_ENABLED, BTNS_BUTTON/*|TBSTYLE_TRANSPARENT*/},
		{0, 0, TBSTATE_ENABLED, TBSTYLE_SEP},
		{13, 14, TBSTATE_ENABLED, TBSTYLE_CHECK/*|TBSTYLE_TRANSPARENT*/}
		};

	mh_Toolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 
        WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT|CCS_NODIVIDER/*|TBSTYLE_TRANSPARENT*/, 0, 0, 0, 0, mh_ToolbarParent, 
        NULL, NULL, NULL); 
 
   SendMessage(mh_Toolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0); 
   SendMessage(mh_Toolbar, TB_SETBITMAPSIZE, 0, MAKELONG(16,16)); 
   TBADDBITMAP bmp = {g_hInstance,IDB_CONMAN1};
   int nFirst = SendMessage(mh_Toolbar, TB_ADDBITMAP, 14, (LPARAM)&bmp);
   //buttons
   SendMessage(mh_Toolbar, TB_ADDBUTTONS, 16, (LPARAM)&buttons);

   SendMessage(mh_Toolbar, TB_AUTOSIZE, 0, 0); 
   SIZE sz; 
   SendMessage(mh_Toolbar, TB_GETMAXSIZE, 0, (LPARAM)&sz);
   RECT rcClient;
   GetWindowRect(_hwndTab, &rcClient);
   MapWindowPoints(NULL, ghWnd, (LPPOINT)&rcClient, 2);
   //int nbWidth = SendMessage(mh_Toolbar, TB_GETBUTTONSIZE, 0, 0) & 0xFFFF;
   //int nHidden = 10;
   SetWindowPos(mh_ToolbarParent, HWND_TOP, 
	   rcClient.right - sz.cx - 2, 0,
	   sz.cx, sz.cy, 0);
   

   //ShowWindow(hwndTB, SW_SHOW); 
   return;
}
