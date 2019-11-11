#include "search_frame.h"

#include <process.h>
#include <strsafe.h>
#include <vector>
#include <wtsapi32.h>
#include <powrprof.h>
#include <assert.h>

#include "common/RCStringUtil.h"
#include "Search/command_line.h"
#include "Search/message_window.h"
#include "Search/search_box.h"
#include "Search/switches.h"
#include "scoped_gdiplus_initializer.h"
#include "ui/miniui/base/RCMiniUIObjectFactory.h"
#include "ui/miniui/graphics/render/RCMiniUIRenderFactorySkia.h"
#include "ui/miniui/res/RCMiniUIFontPool.h"
#include "ui/miniui/res/RCMiniUIResourceManager.h"
#include "ui/miniui/res/RCMiniUIStringPool.h"
#include "ui/miniui/style/RCMiniUIManner.h"
#include "ui/miniui/style/RCMiniUISkinPool.h"
#include "ui/miniui/style/RCMiniUIStylePool.h"
#include "ui/miniui/widget/button/RCMiniUIImageBtn.h"
#include "ui/miniui/widget/RCMiniUITextView.h"
#include "ui/miniui/window/RCMiniUIAppModule.h"

#include "ui/miniui/widget/RCMiniUIImageView.h"
#include "ui/miniui/graphics/bitmap/RCMiniUIBitmap.h"
#include "filesystem/RCFileName.h"
#include "ui/miniui/base/RCMiniUIObjectFactory.h"
#include "ui/miniui/res/RCMiniUIResourceManager.h"
#include "ui/miniui/res/RCMiniUIStringPool.h"


#pragma comment (lib,"Wtsapi32.lib")
#pragma comment (lib,"PowrProf.lib")

BEGIN_NAMESPACE_RC

namespace {


bool ProcessCopyDataMessage(const COPYDATASTRUCT* cds, base::CommandLine* parsed_command_line) {
  static const int32_t min_message_size = 7;
  if (cds->cbData < min_message_size * sizeof(wchar_t) || cds->cbData % sizeof(wchar_t) != 0) {
    assert(false && "invalid WM_COPYDATA cbData length.");
    return false;
  }
  assert(cds->lpData);

  const std::wstring msg(static_cast<wchar_t*>(cds->lpData), cds->cbData / sizeof(wchar_t));
  // 此处解析代码来自Chrome src\chrome\browser\process_singleton_win.cc
  // bool ParserCommandLine 方法
  const std::wstring::size_type first_null = msg.find_first_of(L'\0');
  if (first_null == 0 || first_null == std::wstring::npos) {
    assert(false && "invalid WM_COPYDATA lpData buffer[0]");
    return false;
  }

  if (msg.substr(0, first_null) == L"START") {
    const std::wstring::size_type second_null = msg.find_first_of(L'\0', first_null + 1);
    if (second_null == std::wstring::npos ||
        first_null == msg.length() - 1 ||
        second_null == msg.length()) {
      assert(false && "invalid WM_COPYDATA lpData header.");
      return false;
    }

    const std::wstring cmd_line = msg.substr(first_null + 1, second_null - first_null);
    *parsed_command_line = base::CommandLine(cmd_line.c_str());
    return true;
  }

  return false;
}

//HICON fileIcon(const std::wstring& name, bool folder) {
//  HICON icon = NULL;
//
//  SHFILEINFO info;
//  if (SHGetFileInfoW(name.c_str(),
//    folder ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
//    &info,
//    sizeof(info),
//    SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON)) {
//    icon = info.hIcon;
//  }
//
//  return icon;
//}
HICON fileIcon(const std::wstring& name) {
  HICON icon = NULL;

  //L"C:\\Windows\\System32\\Narrator.exe";
  SHFILEINFO info;
  if (SHGetFileInfoW(name.c_str(),
    NULL,
    &info,
    sizeof(info),
    SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
    icon = info.hIcon;
  }

  return icon;
}
std::wstring Getcut(IShellFolder* folder, PCIDLIST_ABSOLUTE idpl);
HICON GetSystemIcon(IShellFolder* folder, PCIDLIST_ABSOLUTE idpl) {
  IExtractIcon* extract_icon;
  folder->GetUIObjectOf(NULL, 1, &idpl, IID_IExtractIcon, NULL, (void**)&extract_icon);
  wchar_t icon_file[MAX_PATH] = {0};
  int icon_index = 0;
  UINT flag = GIL_SIMULATEDOC;
  extract_icon->GetIconLocation(GIL_FORSHELL, icon_file, MAX_PATH, &icon_index, &flag);
  HICON small_icon = NULL;
  HICON large_icon = NULL;
  UINT size = MAKELONG(32, 32);
  extract_icon->Extract(icon_file, icon_index, &large_icon, &small_icon, size);
  if (!small_icon && !large_icon) {
    //large_icon = fileIcon(L"C:\\Users\\qianw\\Desktop\\HostsTool.exe.lnk");
  }
  return /*small_icon ? small_icon :*/ small_icon;
}

std::wstring Getcut(IShellFolder* folder, PCIDLIST_ABSOLUTE idpl) {
  IShellLink* shell_link;
  folder->GetUIObjectOf(NULL, 1, &idpl, IID_IShellLink, 0, (void**)&shell_link);
  wchar_t icon_file[MAX_PATH] = {0};
  int icon_index = 0;
  //shell_link->GetPath(icon_file, ARRAYSIZE(icon_file), NULL, SLGP_UNCPRIORITY);
  int index = 0;
  HRESULT hr = shell_link->GetIconLocation(icon_file, ARRAYSIZE(icon_file), &index);
  return icon_file;
}
}

RCMINIUIOBJ_IMPLEMENT_DYNAMIC_CREATE(MenuPanel, "menupanel")

MenuPanel::MenuPanel()
    : parent_(nullptr) {
}

MenuPanel::~MenuPanel() {
}

void MenuPanel::SetFrame(SearchFrame* parent) {
  parent_ = parent;
}

bool MenuPanel::OnMouseReleased(uint32_t nFlags, const POINT& point, bool canceled) {
  if (nFlags & MK_RBUTTON && parent_) {
    parent_->RightClickPanel(this, point);
  }

  return __super::OnMouseReleased(nFlags, point, canceled);
}



SearchFrame::SearchFrame() 
    : RCMiniUIFrameWindow(IDR_XML_FRAME_MAIN),
      list_box_(nullptr) {
}

SearchFrame::~SearchFrame() {
}

// static
void SearchFrame::ShowFrame() {
  DWORD dwFlags = ICC_WIN95_CLASSES | ICC_COOL_CLASSES | ICC_DATE_CLASSES | ICC_BAR_CLASSES | ICC_USEREX_CLASSES | ICC_LISTVIEW_CLASSES;
  INITCOMMONCONTROLSEX iccx = {sizeof(INITCOMMONCONTROLSEX), dwFlags};
  ::InitCommonControlsEx(&iccx);
  //阴影需要使用到gdiplus
  ScopedGdiplusInitializer gdiplus_init;

  RCMiniUIAppModule::Instance().SetRenderFactory(new RCMiniUIRenderFactorySkia());
  RCMiniUIAppModule::Instance().Init();

  RCMiniUIResourceManager::GetGolobalFontPool()->SetDefaultFont(_T("Microsoft Yahei"), -12);

  // 载入字符串
  RCMiniUIResourceManager::GetGolobalStringPool()->Load(IDR_XML_GLOBAL_STRING);
  // 载入风格
  RCMiniUIManner::GetGlobalStylePool()->LoadStyles(IDR_XML_GLOBAL_STYLE);
  // 载入皮肤
  RCMiniUIManner::GetGlobalSkinPool()->LoadSkins(IDR_XML_GLOBAL_SKIN);

  SearchFrame search_frame;

  search_frame.Create();
  search_frame.RunFrameWindow();
  RCMiniUIAppModule::Instance().Term();
}

// static
bool SearchFrame::MessageOnlyWindowCallback(void* user_data, UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
  if (message != WM_COPYDATA) {
    return false;
  }

  SearchFrame* self = reinterpret_cast<SearchFrame*>(user_data);
  const COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lparam);
  if (!self || !cds) {
    return false;
  }

  base::CommandLine parsed_command_line(nullptr);
  if (!ProcessCopyDataMessage(cds, &parsed_command_line)) {
    return false;
  }

  self->ProcessCopyDataCommand(parsed_command_line);
  if (result) {
    *result = TRUE;
  }

  return true;
}

void SearchFrame::ProcessCopyDataCommand(const base::CommandLine& command_line) {
  /*if (is_closing_) {
    return;
  }*/

  if (command_line.HasSwitch(switches::kSearchName)) {
    return;
  }

  if (IsIconic()) {
    ShowWindow(SW_RESTORE);
  }
  ::SetForegroundWindow(m_hWnd);
}

void SearchFrame::InitWidgetDialog() {
  // 全局消息窗口
  if (MessageWindow::GetInstance()->FindMessageOnlyWindow() == NULL) {
    MessageWindow::GetInstance()->CreateMessageOnlyWindow(
      this,
      reinterpret_cast<MessageWindow::MessageCallback>(SearchFrame::MessageOnlyWindowCallback));
  }

  list_box_ = (RCMiniUIListBox*)GetViewByID(IDC_FRAME_WINDOW_LISTBOX);
  GetStartMenuProgram();

  list_box_->LayoutChild();
  list_box_->SchedulePaint();
}

void OnShutDown(UINT nID);
void OnSleep();
void WTSTest() {
  MessageBoxA(0, 0, 0, 0);
  PWTS_SESSION_INFO_1 session_info = NULL;
  DWORD count = 0;
  DWORD level = 1;
  ::WTSEnumerateSessionsEx(WTS_CURRENT_SERVER_HANDLE, &level, 0, &session_info, &count);
  for (int i = 0; i < count; ++i, ++session_info) {
    wchar_t out[1024] = {0};
    StringCchPrintf(out, ARRAYSIZE(out), _T("%s, %d, %d"), session_info->pSessionName, session_info->SessionId, session_info->State);
    DWORD size = 0;
    LPWSTR buffer = nullptr;
    ::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSWinStationName, &buffer, &size);
    MessageBox(NULL, buffer, L"", MB_OK);
    MessageBox(NULL, out, L"", MB_OK);
  }
  WTSFreeMemory(session_info);
  return;
  ::WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE);
}

void SearchFrame::OnFirstLayouted() {
  //OnShutDown(2);
  //OnSleep();
  //::LockWorkStation();
  //WTSTest();
}

void SearchFrame::BeforeWindowDestroy() {
}

void SearchFrame::OnSize(UINT type, CSize size) {
  SetMsgHandled(FALSE);

  CRect rect;
  GetClientRect(rect);
  //::SetWindowPos(hwnd_list_ctrl_, NULL, 0, 120, rect.Width(), rect.Height() - 160, 0);
}

void SearchFrame::OnBtnClose() {
  //is_closing_ = true;
  //search_event_.SetEvent();

  PostMessage(WM_CLOSE);
}

void SearchFrame::OnBtnMaximize(RCMiniUIView* view) {
  if (IsZoomed()) {
    RCMiniUIImageBtn* max_btn = dynamic_cast<RCMiniUIImageBtn*>(GetViewByID(IDC_FRAME_WINDOW_SYSBTN_MAXIMIZE));
    if (max_btn) {
      max_btn->SetVisible(true);
      view->SetVisible(false);
      max_btn->SetState(RCMiniUIButton::STATE_NORMAL);
    }
    ShowWindow(SW_RESTORE);
  } else {
    RCMiniUIImageBtn* restore_btn = dynamic_cast<RCMiniUIImageBtn*>(GetViewByID(IDC_FRAME_WINDOW_SYSBTN_RESTORE));
    if (restore_btn) {
      restore_btn->SetVisible(true);
      view->SetVisible(false);
      restore_btn->SetState(RCMiniUIButton::STATE_NORMAL);
    }
    ShowWindow(SW_MAXIMIZE);
  }
}

void SearchFrame::OnBtnMinimize() {
  ShowWindow(SW_MINIMIZE);
}

bool SearchFrame::IsFolderFromItem(IShellItem* item) {
  SFGAOF flags = 0;
  HRESULT hr = item->GetAttributes(SFGAO_FOLDER | SFGAO_HIDDEN, &flags);
  if (FAILED(hr) || !(flags & SFGAO_FOLDER)) {
    return false;
  }

  return true;
}

void SearchFrame::TraversalProgram(IShellFolder* folder, RCMiniUIView* parent, bool child, int level = 1) {
  HRESULT hr = S_OK;
  IEnumIDList* pEnumIDList = NULL;
  SHCONTF SHFlag = /*SHCONTF_INIT_ON_FIRST_NEXT |*/ SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN;

  hr = folder->EnumObjects(NULL, SHFlag, &pEnumIDList);
  if (FAILED(hr)) {
    return;
  }
  PIDLIST_ABSOLUTE pidl = NULL;
  while (NOERROR == pEnumIDList->Next(1, &pidl, NULL)) {
    //LPWSTR name = nullptr;
    STRRET display_name = {0};
    folder->GetDisplayNameOf(pidl, SIGDN_NORMALDISPLAY, &display_name);
    STRRET system_path_name = {0};
    folder->GetDisplayNameOf(pidl, SIGDN_FILESYSPATH, &system_path_name);
    ULONG attr = SFGAO_FOLDER | SFGAO_STREAM | SFGAO_FILESYSTEM | SFGAO_LINK;
    PCIDLIST_ABSOLUTE arr = pidl;
    folder->GetAttributesOf(1, &arr, &attr);
    bool is_folder = attr & SFGAO_FOLDER ? true : false;
    bool is_link = attr & SFGAO_LINK ? true : false;
    //shell_item_tmp->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
    //std::wstring path = RC_QUALIFIER RCFileName::JoinFileName(program_Path, display_name.pOleStr);
    //SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&shell_item_tmp));
    
    std::wstring panel_xml = RCMiniUIResourceManager::GetGolobalStringPool()->Get(IDS_BOX_PANEL_XML_TO_FORMAT);
    std::wstring xml;
    RCStringUtil::Format(xml, panel_xml.c_str(), 10+ 20 * (level -1), 35 + 20 * (level -1), display_name.pOleStr);
    MenuPanel* box_panel = dynamic_cast<MenuPanel*>(RCMiniUIObjectFactory::CreateViewByString(xml));
    box_panel->SetFrame(this);
    box_panel->SetUserData((void*)system_path_name.pOleStr);
    auto image = (RCMiniUIImageView*)box_panel->GetViewByID(12003);
    RC_QUALIFIER RCMiniUIRenderFactory* render_factory = RC_QUALIFIER RCMiniUIAppModule::Instance().GetRenderFactory();
    if (!render_factory) {
      return;
    }

    RC_QUALIFIER RCMiniUIBitmap* result = render_factory->CreateBitmap();
    if (!result) {
      return;
    }

    if (is_folder) {
      IShellFolder* sub_folder = NULL;
      folder->BindToObject(pidl, NULL, IID_PPV_ARGS(&sub_folder));
      //TraversalProgram(sub_folder, box_panel, true, level+1);
    }
    /*if (!is_folder) {
      Getcut(folder, pidl);
    }*/
    
    /*result->LoadFromFile(icon);*/
    HICON icon = NULL;
    if (is_link) {
      icon = fileIcon(system_path_name.pOleStr);
    } else {
      icon = GetSystemIcon(folder, pidl);
    }
    ////
    if (icon) {
      result->LoadFromHIcon(icon);
      image->ResetImage(result);
    }
    
    if (!child) {
      RCMiniUIListBox* list_box = (RCMiniUIListBox*)parent;
      list_box->AddChildPanel(box_panel);
    } else {
      MenuPanel* box_panel_parent = (MenuPanel*)parent;
      
      box_panel_parent->AddChildViewToPanel(box_panel);
    }
  }
}

void test() {
  HWND hWndDesktop = NULL;
  hWndDesktop = FindWindow(_T("Progman"), NULL);
  hWndDesktop = FindWindowEx(hWndDesktop, NULL, _T("SHELLDLL_DefView"), NULL);

  // Get the desktop icon list control handle
  HWND hWnd = FindWindowEx(hWndDesktop, NULL, _T("SysListView32"), NULL);
  if (NULL == hWnd) {
    return;
  }
}

void SearchFrame::GetStartMenuProgram() {
  //test();
  PIDLIST_ABSOLUTE id_list;
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  SHGetKnownFolderIDList(FOLDERID_Desktop, 0, NULL, &id_list);

  IShellItem* shell_item = NULL;
  hr = SHCreateItemFromIDList(id_list, IID_PPV_ARGS(&shell_item));
  if (FAILED(hr)) {
    return;
  }
  if (!IsFolderFromItem(shell_item)) {
    return;
  }

  IShellFolder* folder = NULL;
  shell_item->BindToHandler(NULL, BHID_SFObject, IID_PPV_ARGS(&folder));
  //IShellFolder *pDesktopFolder = NULL;

  //SHGetDesktopFolder(&pDesktopFolder);
  TraversalProgram(folder, list_box_, false);
}

void SearchFrame::OnClickPanel(RCMiniUIView* view) {
  MenuPanel* panel = (MenuPanel*)view;
  if (panel != NULL) {
    if (panel->GetExpand()) {
      panel->SetExpand(false);
    } else {
      panel->SetExpand(true);
    }
  }
}

void SearchFrame::RightClickPanel(RCMiniUIView* view, const POINT& point) {
  MenuPanel* panel = (MenuPanel*)view;
  if (panel != NULL) {
    LPWSTR path = (LPWSTR)panel->GetUserData();
    IShellItem* shell_item = NULL;
    PIDLIST_ABSOLUTE id_list = ILCreateFromPath(path);
    HRESULT hr = SHCreateItemFromIDList(id_list, IID_PPV_ARGS(&shell_item));
    if (FAILED(hr)) {
      return;
    }
    ::CoTaskMemFree(id_list);
    LPCONTEXTMENU context_menu = NULL;
    hr = shell_item->BindToHandler(NULL, BHID_SFUIObject, IID_PPV_ARGS(&context_menu));
    if (FAILED(hr)) {
      shell_item->Release();
      return;
    }
    
    HMENU menu = ::CreatePopupMenu();
    context_menu->QueryContextMenu(menu,
                                   0,
                                   1,
                                   0x7FFF,
                                   CMF_NORMAL |
                                   CMF_EXPLORE |
                                   CMF_CANRENAME);

    CPoint pt;
    ::GetCursorPos(&pt);
    
    UINT command_id = ::TrackPopupMenu(menu,
                                       TPM_LEFTALIGN |
                                       TPM_RIGHTBUTTON |
                                       TPM_VERTICAL |
                                       TPM_RETURNCMD,
                                       pt.x,
                                       pt.y,
                                       0,
                                       m_hWnd,
                                       NULL);

    if (command_id >= 1 && command_id <= 0x7FFF) {
      CMINVOKECOMMANDINFO cmi = {0};
      cmi.cbSize = sizeof(CMINVOKECOMMANDINFO);
      cmi.lpVerb = (LPSTR)MAKEINTRESOURCE(command_id - 1);
      cmi.nShow = SW_SHOWNORMAL;
      context_menu->InvokeCommand(&cmi);	// execute related command
      command_id = 0;
    }

    ::DestroyMenu(menu);
    shell_item->Release();
  }
}

void OnShutDown(UINT nID) {

  HANDLE hToken;              // handle to process token 
  TOKEN_PRIVILEGES tkp;       // pointer to token structure 


  OpenProcessToken(GetCurrentProcess(),
                   TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);

  // Get the LUID for shutdown privilege. 

  LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
                       &tkp.Privileges[0].Luid);

  tkp.PrivilegeCount = 1;  // one privilege to set    
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  // Get shutdown privilege for this process. 

  AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                        (PTOKEN_PRIVILEGES)NULL, 0);

  // Cannot test the return value of AdjustTokenPrivileges. 

  //if (GetLastError() != ERROR_SUCCESS) 
  //  MessageBox("AdjustTokenPrivileges enable failed."); 
  
  int nArgument;

  switch (nID) {

    case 0:nArgument = EWX_REBOOT; break;
    case 1:nArgument = EWX_SHUTDOWN; break;
    case 2:nArgument = EWX_POWEROFF; break;
    case 3:nArgument = EWX_LOGOFF;

  }

  //checking for force 
    ::ExitWindowsEx(nArgument, 0);
  // MessageBox("hi");

}

void OnSleep() {
  ::SetSuspendState(TRUE, FALSE, FALSE);
}

END_NAMESPACE_RC