#include "Search/message_window.h"

#include <assert.h>

#include "Search/ui_message.h"
#include "resource.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {
const wchar_t kGameHallBrowserMessageOnlyWindowClassName[] = L"2345Search_MessageWindow";
const wchar_t kGameHallBrowserMessageOnlyWindowTitleName[] = L"com2345.search.unique.running.title";

}  // namespace


class MessageWindow::WindowClass {
 public:
  WindowClass()
      : atom_(0),
        instance_(reinterpret_cast<HINSTANCE>(&__ImageBase)) {
    WNDCLASSEXW window_class = { 0 };
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = MessageWindow::WindowProc;
    window_class.hInstance = instance_;
    window_class.lpszClassName = kGameHallBrowserMessageOnlyWindowClassName;

    atom_ = ::RegisterClassExW(&window_class);

    if (atom_ == 0) {
      assert(false && "failed to register gamehall message only window.");
    }
  }

  ~WindowClass() {
    if (atom_ != 0) {
      BOOL result = ::UnregisterClassW(MAKEINTATOM(atom_), instance_);
      assert(result);
    }
  }

  ATOM atom() const {
    return atom_;
  }

  HINSTANCE instance() const {
    return instance_;
  }

 private:
  ATOM atom_;
  HINSTANCE instance_;

  DISALLOW_COPY_AND_ASSIGN(WindowClass);

};  // class MessageWindow::WindowClass


MessageWindow::WindowClass* MessageWindow::class_instance_ = nullptr;

MessageWindow::MessageWindow()
    : callback_(nullptr),
      window_(NULL),
      user_data_(nullptr) {
}

MessageWindow::~MessageWindow() {
  if (window_ != NULL) {
    BOOL result = ::DestroyWindow(window_);
    assert(result);
  }
}

HWND MessageWindow::FindMessageOnlyWindow() {
  return ::FindWindowExW(HWND_MESSAGE, NULL, kGameHallBrowserMessageOnlyWindowClassName, kGameHallBrowserMessageOnlyWindowTitleName);
}

MessageWindow* MessageWindow::GetInstance() {
  static MessageWindow s_message_window_instance;
  return &s_message_window_instance;
}

bool MessageWindow::CreateMessageOnlyWindow(void* user_data, MessageWindow::MessageCallback callback) {
  user_data_ = user_data;
  callback_ = callback;

  if (class_instance_ == nullptr) {
    class_instance_ = new WindowClass();
  }

  window_ = ::CreateWindowW(MAKEINTATOM(class_instance_->atom()),
                            kGameHallBrowserMessageOnlyWindowTitleName,
                            0,
                            0,
                            0,
                            0,
                            0,
                            HWND_MESSAGE,
                            NULL,
                            class_instance_->instance(),
                            this);
  if (!window_) {
    assert(false && "failed to create gamehall message only window.");
    return false;
  }

  return true;
}

HWND MessageWindow::GetWindow() const {
  return window_;
}

LRESULT CALLBACK MessageWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  MessageWindow* self = reinterpret_cast<MessageWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (message) {
    case WM_CREATE: {
      CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = reinterpret_cast<MessageWindow*>(cs->lpCreateParams);
      // Make |hwnd| available to the message handler. At this point the control
      // hasn't returned from CreateWindow() yet.
      self->window_ = hwnd;
      ::SetLastError(ERROR_SUCCESS);
      LONG_PTR result = ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
      assert(result != 0 || ::GetLastError() == ERROR_SUCCESS);
      break;
    }
    case WM_DESTROY: {
      ::SetLastError(ERROR_SUCCESS);
      LONG_PTR result = ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, NULL);
      assert(result != 0 || ::GetLastError() == ERROR_SUCCESS);
      break;
    }
    case WM_TIMER: {
      break;
    }
    case WM_HOTKEY: {
      break;
    }
  }

  // 通知回调处理消息
  if (self) {
    LRESULT message_result = 0;
    if (self->callback_ && self->callback_(self->user_data_, message, wparam, lparam, &message_result)) {
      return message_result;
    }
  }

  return ::DefWindowProcW(hwnd, message, wparam, lparam);
}
