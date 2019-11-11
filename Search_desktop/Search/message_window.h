#ifndef GAMEHALLMAIN_UI_NATIVE_WINDOW_MESSAGE_WINDOW_H_
#define GAMEHALLMAIN_UI_NATIVE_WINDOW_MESSAGE_WINDOW_H_ 1


#include <Windows.h>

#include "Search/macros.h"


class MessageWindow {
 public:
  typedef bool (*MessageCallback)(void* user_data, UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result);

  static HWND FindMessageOnlyWindow();

  static MessageWindow* GetInstance();

  bool CreateMessageOnlyWindow(void* user_data, MessageCallback callback);

  HWND GetWindow() const;

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  MessageWindow();
  ~MessageWindow();
  DISALLOW_COPY_AND_ASSIGN(MessageWindow);

 private:
  class WindowClass;
  friend class WindowClass;
  static WindowClass* class_instance_;

  MessageCallback callback_;
  HWND window_;
  void* user_data_;
};  // class MessageWindow

#endif  // GAMEHALLMAIN_UI_NATIVE_WINDOW_MESSAGE_WINDOW_H_
