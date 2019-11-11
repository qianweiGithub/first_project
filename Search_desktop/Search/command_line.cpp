#include "Search/command_line.h"

#include <algorithm>
#include <Windows.h>
#include <ShellAPI.h>

#include "common/RCStringConvert.h"
#include "common/RCStringToken.h"
#include "common/RCStringUtil.h"


namespace base {

namespace {

const std::wstring kSwitchValueSeparator = L"=";
const std::wstring kSwitchPrefix = L"--";

}  // namespace

CommandLine* CommandLine::current_process_commandline_ = nullptr;


CommandLine::CommandLine() {
}

CommandLine::CommandLine(const wchar_t* command_line) {
  if (command_line != nullptr) {
    ParseFromString(command_line);
  }
}

CommandLine::~CommandLine() {
}

void CommandLine::Init() {
  if (current_process_commandline_) {
    return;
  }

  current_process_commandline_ = new CommandLine();
  current_process_commandline_->ParseFromString(::GetCommandLineW());
}

CommandLine* CommandLine::ForCurrentProcess() {
  return current_process_commandline_;
}

bool CommandLine::HasSwitch(const std::string& switch_string) const {
  std::wstring switch_key = RC_QUALIFIER RCStringConvert::MultiByteToUnicodeString(switch_string);
  return HasSwitch(switch_key);
}

bool CommandLine::HasSwitch(const std::wstring& switch_string) const {
  return argv_.find(switch_string) != argv_.end();
}

void CommandLine::AppendSwitch(const std::string& switch_string) {
  std::wstring switch_key = RC_QUALIFIER RCStringConvert::MultiByteToUnicodeString(switch_string);
  AppendSwitch(switch_key);
}

void CommandLine::AppendSwitch(const std::wstring& switch_string) {
  argv_.insert(std::make_pair(switch_string, std::wstring()));
}

void CommandLine::AppendSwitchAndValue(const std::string& switch_string, const std::string& value) {
  std::wstring switch_key = RC_QUALIFIER RCStringConvert::MultiByteToUnicodeString(switch_string);
  std::wstring switch_value = RC_QUALIFIER RCStringConvert::MultiByteToUnicodeString(value);
  AppendSwitchAndValue(switch_key, switch_value);
}

void CommandLine::AppendSwitchAndValue(const std::wstring& switch_string, const std::wstring& value) {
  argv_.insert(std::make_pair(switch_string, value));
}

std::string CommandLine::GetSwitchValueASCII(const std::string& switch_string) const {
  std::wstring switch_key = RC_QUALIFIER RCStringConvert::MultiByteToUnicodeString(switch_string);
  return GetSwitchValueASCII(switch_key);
}

std::string CommandLine::GetSwitchValueASCII(const std::wstring& switch_string) const {
  std::map<std::wstring, std::wstring>::const_iterator result = argv_.find(switch_string);
  if (result == argv_.end()) {
    return std::string();
  }

  return RC_QUALIFIER RCStringConvert::UnicodeStringToMultiByte(result->second);
}

std::wstring CommandLine::GetSwitchValue(const std::string& switch_string) const {
  std::wstring switch_key = RC_QUALIFIER RCStringConvert::MultiByteToUnicodeString(switch_string);
  return GetSwitchValue(switch_key);
}

std::wstring CommandLine::GetSwitchValue(const std::wstring& switch_string) const {
  std::map<std::wstring, std::wstring>::const_iterator result = argv_.find(switch_string);
  if (result == argv_.end()) {
    return std::wstring();
  }

  return result->second;
}

void CommandLine::GetSwitches(std::vector<std::wstring>& switches) const {
  switches.clear();
  for (auto position : argv_) {
    switches.push_back(position.first);
  }
}

void CommandLine::GetNonPrefixedSwitches(std::vector<std::wstring>& switches) const {
  switches.clear();
  for (auto position : argv_) {
    if (position.first.find(kSwitchPrefix) != 0) {
      switches.push_back(position.first);
    }
  }
}

void CommandLine::ParseFromString(const std::wstring& command_line) {
  std::wstring command_line_string = command_line;
  RC_QUALIFIER RCStringUtil::Trim(command_line_string);
  if (command_line_string.empty()) {
    return;
  }

  int argc = 0;
  wchar_t** argv = ::CommandLineToArgvW(command_line_string.c_str(), &argc);
  for (int i = 0; i < argc; ++i) {
    std::wstring command = argv[i];
    std::wstring::size_type position = command.find(kSwitchPrefix);
    if (position == 0) {
      position = command.find(kSwitchValueSeparator);
      if (position == std::wstring::npos) {
        argv_.insert(std::make_pair(command, std::wstring()));
      } else {
        argv_.insert(std::make_pair(command.substr(0, position), command.substr(position + 1)));
      }
    } else {
      argv_.insert(std::make_pair(command, std::wstring()));
    }
  }

  ::LocalFree(argv);
}

std::wstring CommandLine::ToString() const {
  std::wstring command_string;
  for (const auto& cmd_unit : argv_) {
    if (!cmd_unit.second.empty()) {
      command_string.append(cmd_unit.first.c_str());
      command_string.append(L"=", 1);
      command_string.append(cmd_unit.second.c_str());
    } else {
      command_string.append(cmd_unit.first.c_str());
    }
    command_string.append(L" ", 1);
  }
  if (command_string.length() > 2) {
    command_string = command_string.substr(0, command_string.length() - 1);
  }

  return command_string;
}

}  // namespace base
