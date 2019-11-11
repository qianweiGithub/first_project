#include <tchar.h>
#include <Windows.h>
#include <wuapi.h>
#include <iostream>
#include <ATLComTime.h>
#include <wtsapi32.h>


#include "Search/command_line.h"
#include "Search/everything_helper.h"
#include "Search/message_window.h"
#include "Search/search_frame.h"

#include "filesystem/RCFile.h"
#include "filesystem/RCFileName.h"
#include "filesystem/RCFilePathUtils.h"
#include "filesystem/RCFindFile.h"
#include "filesystem/RCFileInfo.h"

#include <strsafe.h>

#pragma comment (lib,"Wtsapi32.lib")

namespace {

const uint32_t kSendCopyDataMessageTimeoutInMS = 15 * 1000;

bool AttemptFastNotify(const base::CommandLine& command_line) {
  HWND only_window = MessageWindow::FindMessageOnlyWindow();
  if (!only_window) {
    return false;
  } else {
    DWORD main_pid = 0;
    ::GetWindowThreadProcessId(only_window, &main_pid);
    if (main_pid == 0) {
      return false;
    }
    std::wstring to_send(L"START\0", 6);
    to_send.append(command_line.ToString());
    to_send.append(L"\0", 1);

    ::AllowSetForegroundWindow(main_pid);
    COPYDATASTRUCT cds = {0};
    cds.cbData = static_cast<DWORD>((to_send.length() + 1) * sizeof(wchar_t));
    cds.lpData = const_cast<wchar_t*>(to_send.c_str());
    DWORD_PTR result = 0;
    if (::SendMessageTimeoutW(only_window,
      WM_COPYDATA,
      NULL,
      reinterpret_cast<LPARAM>(&cds),
      SMTO_ABORTIFHUNG,
      kSendCopyDataMessageTimeoutInMS,
      &result)) {
      return !!result;
    }
    return false;
  }
}

bool IsFolderFromFolder(IShellFolder* folder, PCIDLIST_ABSOLUTE pidl) {
  SFGAOF flags = 0;
  HRESULT hr = folder->GetAttributesOf(1, &pidl, &flags);
  if (FAILED(hr) || !(flags & SFGAO_FOLDER)) {
    return false;
  }

  return true;
}

bool IsFolderFromItem(IShellItem* item) {
  SFGAOF flags = 0;
  HRESULT hr = item->GetAttributes(SFGAO_FOLDER | SFGAO_HIDDEN, &flags);
  if (FAILED(hr) || !(flags & SFGAO_FOLDER)) {
    return false;
  }

  return true;
}

void TraversalProgram(IShellFolder* folder, std::wstring& program_Path, int level) {
  HRESULT hr = S_OK;
  IEnumIDList* pEnumIDList = NULL;
  SHCONTF SHFlag = SHCONTF_FOLDERS | SHCONTF_INIT_ON_FIRST_NEXT | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN;

  hr = folder->EnumObjects(NULL, SHFlag, &pEnumIDList);
  if (FAILED(hr)) {
    return;
  }
  PIDLIST_ABSOLUTE pidl = NULL;
  IShellItem *shell_item_tmp = NULL;
  std::wstring space;
  for (int i = 1; i < level; ++i) {
    space += L"----";
  }
  while (NOERROR == pEnumIDList->Next(1, &pidl, NULL)) {
    //LPWSTR name = nullptr;
    STRRET name = {0};
    folder->GetDisplayNameOf(pidl, SIGDN_NORMALDISPLAY, &name);
    //shell_item_tmp->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
    std::wstring path = RC_QUALIFIER RCFileName::JoinFileName(program_Path, name.pOleStr);
    std::wstring print = space + path;
    OutputDebugString(print.c_str());
    SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&shell_item_tmp));
    if (IsFolderFromItem(shell_item_tmp)) {
      IShellFolder* sub_folder = NULL;
      folder->BindToObject(pidl, NULL, IID_PPV_ARGS(&sub_folder));
      TraversalProgram(sub_folder, path, level + 1);
    }
    continue;
  }
}

bool GetStartMenuProgram() {
  LPWSTR program_Path = NULL;
  //HRESULT result = 
  PIDLIST_ABSOLUTE id_list;
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); //must add this!
  ::SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &program_Path);
  SHGetKnownFolderIDList(FOLDERID_Programs, 0, NULL, &id_list);
  IShellItem* shell_item = NULL;
  hr = SHCreateItemFromIDList(id_list, IID_PPV_ARGS(&shell_item));
  if (FAILED(hr)) {
    return false;
  }
  if (!IsFolderFromItem(shell_item)) {
    return false;
  }
  std::wstring path(program_Path);
  IShellFolder* folder = NULL;
  shell_item->BindToHandler(NULL, BHID_SFObject, IID_PPV_ARGS(&folder));

  TraversalProgram(folder, path, 1);
  /*IShellFolder* root_folder = NULL;
  shell_item->BindToHandler(NULL, BHID_SFObject, IID_PPV_ARGS(&root_folder));

  IEnumIDList* pEnumIDList = NULL;
  SHCONTF SHFlag = SHCONTF_FOLDERS | SHCONTF_INIT_ON_FIRST_NEXT | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN;

  hr = root_folder->EnumObjects(NULL, SHFlag, &pEnumIDList);

  PIDLIST_ABSOLUTE pidl = NULL;
  HTREEITEM hItem = NULL;
  IShellItem *shell_item_tmp = NULL;
  while (NOERROR == pEnumIDList->Next(1, &pidl, NULL)) {
    SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&shell_item_tmp));
    LPWSTR name = nullptr;
    shell_item_tmp->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
    std::wstring path = RC_QUALIFIER RCFileName::JoinFileName(program_Path, name);
    OutputDebugString(path.c_str());
    if (IsFolder(shell_item_tmp)) {
      TraversalProgram(shell_item_tmp);
    }
    continue;
  }*/
  //
  
  
  
  
  
  
  //RC_QUALIFIER RCFileInfo file_info;
  //RC_QUALIFIER RCFindFile file_finder;
  //std::wstring script_root_dir = startMenuPath;
  //RC_QUALIFIER RCFileName::NormalizePath(script_root_dir, RC_QUALIFIER RCFileName::NORMALIZE_DIR_PREFIX);

  //// 遍历Script目录下所有的dat文件，排除info.dat，不在录制列表中的dat文件认为不再使用，删除之
  //if (file_finder.FindFirst(script_root_dir + L"*", file_info)) {
  //  if (!file_info.IsDots()) {
  //    std::wstring current_path = RC_QUALIFIER RCFileName::JoinFileName(script_root_dir, file_info.GetFileName());
  //  }

  //  PIDLIST_ABSOLUTE pidl; //=LPITEMIDLIST
  //  IShellItem2 *shell_item = NULL;
  //  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); //must add this!
  //  while (file_finder.FindNext(file_info)) {
  //    if (file_info.IsDots()) {
  //      continue;
  //    }

  //    std::wstring current_path = RC_QUALIFIER RCFileName::JoinFileName(script_root_dir, file_info.GetFileName());
  //    PIDLIST_ABSOLUTE id_list = ::ILCreateFromPath(current_path.c_str());
  //    hr = SHCreateItemFromIDList(id_list, IID_PPV_ARGS(&shell_item));
  //    LPWSTR name = nullptr;
  //    shell_item->GetDisplayName(SIGDN_NORMALDISPLAY, &name);
  //    current_path = RC_QUALIFIER RCFileName::JoinFileName(script_root_dir, name);
  //    OutputDebugString(current_path.c_str());
  //  }
  //}

  //file_finder.Close();
  return true;
}

int update() {
  HRESULT hr;
  hr = CoInitialize(NULL);

  IUpdateSession* update_session;
  IUpdateSearcher* searcher;
  ISearchResult* results;
  BSTR criteria = ::SysAllocString(L"RebootRequired=1");
  
  hr = ::CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&update_session);
  if (FAILED(hr)) {
    return 0;
  }

  hr = update_session->CreateUpdateSearcher(&searcher);
  if (FAILED(hr)) {
    return 0;
  }

  hr = searcher->Search(criteria, &results);
  if (FAILED(hr)) {
    return 0;
  }

  ::SysFreeString(criteria);

  
  IUpdateCollection *updateList;
  IUpdate *updateItem;
  LONG updateSize;
  BSTR updateName;
  DATE retdate;

  results->get_Updates(&updateList);
  updateList->get_Count(&updateSize);
  if (updateSize > 0) {
    MessageBoxA(0,"需要重启",0,0);
  } else {
    MessageBoxA(0, "不需要重启", 0, 0);
  }

  for (LONG i = 0; i < updateSize; i++) {
    updateList->get_Item(i, &updateItem);
    updateItem->get_Title(&updateName);
    updateItem->get_LastDeploymentChangeTime(&retdate);
    COleDateTime odt;
    odt.m_dt = retdate;
  }
  ::CoUninitialize();
  return 0;
}
/*
void InitLog() {
  FLAGS_log_dir = "D:\\code\\Search_startmenu\\build\\Debug";
  google::InitGoogleLogging("D:\\code\\Search_startmenu\\build\\Debug\\Search.exe");
  google::SetLogDestination(google::GLOG_INFO, "D:\\code\\Search_startmenu\\build\\Debug\\INFO_");
  google::SetStderrLogging(google::GLOG_INFO);
  google::SetLogFilenameExtension("log_");
  FLAGS_colorlogtostderr = true;  // Set log color
  FLAGS_logbufsecs = 0;  // Set log output speed(s)
  FLAGS_max_log_size = 1024;  // Set max log file size
  FLAGS_stop_logging_if_full_disk = true;  // If disk is full

  //char str[20] = "hello log!";
  //LOG(INFO) << str;
  //LOG(INFO) << "info test" << "hello log!";  //输出一个Info日志
  //LOG(WARNING) << "warning test";  //输出一个Warning日志
  //LOG(ERROR) << "error test";  //输出一个Error日志
}

void ShutdownLog() {
  google::ShutdownGoogleLogging();
}
*/
void WTSTest() {
  // 判断当前session是否是远程连接
  DWORD size = 0;
  LPWSTR buffer = nullptr;
  ::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSClientProtocolType, &buffer, &size);
  USHORT protocol_type = (USHORT)*buffer;
  if (protocol_type == 2) {
    // 2是远程连接，0是控制台
    MessageBoxA(NULL, "断开连接", "", MB_OK);
    ::WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE);
  }
  ::WTSFreeMemory(buffer);
}

}

int WINAPI _tWinMain(HINSTANCE instance, HINSTANCE previous_instance, LPTSTR cmdline, int show_cmd) {
  UNREFERENCED_PARAMETER(previous_instance);
 // MessageBoxA(0,0,0,0);
  //WTSTest();
  //update();
  //return 0;
  //GetStartMenuProgram();
  //return 0;
  // 解析命令行
  base::CommandLine command_line(cmdline);
  if (AttemptFastNotify(command_line)) {
    return 0;
  }

  /*if (!EverythingHelper::Initialize()) {
    return 0;
  }*/

  RC_QUALIFIER SearchFrame::ShowFrame();
  return 0;
}