#ifndef SEARCH_SEARCH_FRAME_H_
#define SEARCH_SEARCH_FRAME_H_ 1


#include "Search/ui_message.h"
#include "thread/RCEvent.h"
#include "ui/miniui/widget/listbox/RCMiniUIListBox.h"
#include "ui/miniui/window/RCMiniUIFrameWindow.h"
#include "resource.h"


namespace base {
  class CommandLine;
}

BEGIN_NAMESPACE_RC
class RCMiniUIListBox;
class SearchBox;
class SearchFrame;

class MenuPanel
  : public RCMiniUIBoxPanel {
  RCMINIUIOBJ_DECLARE_CLASS_NAME(MenuPanel, "menupanel")
public:
  MenuPanel();
  ~MenuPanel();

  void SetFrame(SearchFrame* parent);
  // 
  bool OnMouseReleased(uint32_t nFlags, const POINT& point, bool canceled) override;
private:
  SearchFrame* parent_;
};

class SearchFrame : public RCMiniUIFrameWindow {
 public:
  SearchFrame();
  ~SearchFrame();

  void RightClickPanel(RCMiniUIView* view, const POINT& point);
  static void ShowFrame();
  static void QueryThreadProc(void* param);

 public:

  BEGIN_MSG_MAP_EX(SearchFrame)
    MSG_WM_SIZE(OnSize)
    MSG_RCMINIUIOBJ_NOTIFY(IDC_RICHVIEW_WIN)
    CHAIN_MSG_MAP(RCMiniUIFrameWindow)
    REFLECT_NOTIFICATIONS_EX()
    END_MSG_MAP()

    RCMINIUIOBJ_NOTIFY_MAP(IDC_RICHVIEW_WIN)
      RCMINIUIOBJ_NOTIFY_ID_COMMAND(IDC_FRAME_WINDOW_SYSBTN_CLOSE, OnBtnClose)
      RCMINIUIOBJ_NOTIFY_ID_COMMAND_VIEW(IDC_FRAME_WINDOW_SYSBTN_MAXIMIZE, OnBtnMaximize)
      RCMINIUIOBJ_NOTIFY_ID_COMMAND_VIEW(IDC_FRAME_WINDOW_SYSBTN_RESTORE, OnBtnMaximize)
      RCMINIUIOBJ_NOTIFY_ID_COMMAND(IDC_FRAME_WINDOW_SYSBTN_MINIMIZE, OnBtnMinimize)
      RCMINIUIOBJ_NOTIFY_ID_COMMAND_VIEW(IDC_FRAME_WINDOW_LISTBOX_BOX_PANEL, OnClickPanel)
    RCMINIUIOBJ_NOTIFY_MAP_END()

 private:
  static bool MessageOnlyWindowCallback(void* user_data, UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result);
  void ProcessCopyDataCommand(const base::CommandLine& command_line);

  void InitWidgetDialog() override;
  void OnFirstLayouted() override;
  void BeforeWindowDestroy() override;

  void OnSize(UINT type, CSize size);

  void OnBtnClose();
  void OnBtnMaximize(RCMiniUIView* view);
  void OnBtnMinimize();

  bool IsFolderFromItem(IShellItem* item);
  void TraversalProgram(IShellFolder* folder, RCMiniUIView* parent, bool child, int level);
  void GetStartMenuProgram();

  void OnClickPanel(RCMiniUIView* view);

 private:
  RCMiniUIListBox* list_box_;
};

END_NAMESPACE_RC

#endif  // SEARCH_SEARCH_FRAME_H_