#ifndef SEARCH_COMMAND_LINE_H_
#define SEARCH_COMMAND_LINE_H_ 1


#include <map>
#include <string>
#include <vector>


namespace base {

class CommandLine {
 public:
  explicit CommandLine(const wchar_t* command_line);
  ~CommandLine();

  static void Init();
  static CommandLine* ForCurrentProcess();

  bool HasSwitch(const std::string& switch_string) const;
  bool HasSwitch(const std::wstring& switch_string) const;

  void AppendSwitch(const std::string& switch_string);
  void AppendSwitch(const std::wstring& switch_string);

  void AppendSwitchAndValue(const std::string& switch_string, const std::string& value);
  void AppendSwitchAndValue(const std::wstring& switch_string, const std::wstring& value);

  std::string GetSwitchValueASCII(const std::string& switch_string) const;
  std::string GetSwitchValueASCII(const std::wstring& switch_string) const;
  std::wstring GetSwitchValue(const std::string& switch_string) const;
  std::wstring GetSwitchValue(const std::wstring& switch_string) const;

  void GetSwitches(std::vector<std::wstring>& switches) const;
  void GetNonPrefixedSwitches(std::vector<std::wstring>& switches) const;

  std::wstring ToString() const;

 private:
  CommandLine();

  void ParseFromString(const std::wstring& command_line);

 private:

  static CommandLine* current_process_commandline_;

  std::map<std::wstring, std::wstring> argv_;

};  // class CommandLine

}  // namespace base

#endif  // SEARCH_COMMAND_LINE_H_
