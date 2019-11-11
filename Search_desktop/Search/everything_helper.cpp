#include "Search/everything_helper.h"

#include <string>
#include <windows.h>

#include "common/RCStringUtil.h"
#include "Search/command_line.h"


namespace {
const wchar_t kInstanceName[] = L"abc";

ServiceStatus CheckEverythingService() {
  std::wstring service_name;
  RC_QUALIFIER RCStringUtil::Format(service_name, L"Everything (%s)", kInstanceName);
  // Get a handle to the SCM database. 
  SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);  
  if (NULL == sc_manager) {
    return kServiceStatus_AccessFailed;
  }

  // Get a handle to the service.
  SC_HANDLE sc_service = OpenService(sc_manager, service_name.c_str(), SERVICE_QUERY_STATUS);  
  if (sc_service == NULL) {
    int error = ::GetLastError();
    CloseServiceHandle(sc_manager);
    return ERROR_SERVICE_DOES_NOT_EXIST == error ? kServiceStatus_ServiceNotInstalled : kServiceStatus_AccessFailed;
  }

  CloseServiceHandle(sc_manager);
  CloseServiceHandle(sc_service);

  return kServiceStatus_ServiceInstalled;
}

bool InstallEverythingService() {
  std::wstring command_line_param;
  RC_QUALIFIER RCStringUtil::Format(command_line_param, L"-install-service -instance %s", kInstanceName);
  ::ShellExecute(NULL, L"open", L"F:/Everything-1.4.1.809b.x64/Everything.exe", command_line_param.c_str(), NULL, NULL);

  return true;
}

bool LaunchInstance() {
  std::wstring command_line_param;
  RC_QUALIFIER RCStringUtil::Format(command_line_param, L"-startup -instance %s", kInstanceName);
  ::ShellExecute(NULL, L"open", L"F:/Everything-1.4.1.809b.x64/Everything.exe", command_line_param.c_str(), NULL, NULL);

  return true;
}

}

bool EverythingHelper::Initialize() {
  ServiceStatus service_status = CheckEverythingService();
  bool result = false;
  switch (service_status) {
    case kServiceStatus_ServiceInstalled:
      result = true;
      break;
    case kServiceStatus_ServiceNotInstalled:
      result = InstallEverythingService();
      break;
    case kServiceStatus_Unknown:
    case kServiceStatus_AccessFailed:
    default:
      break;
  }

  LaunchInstance();

  return result;
}