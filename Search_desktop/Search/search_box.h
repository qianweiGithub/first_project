#ifndef SEARCH_SEARCH_BOX_H_
#define SEARCH_SEARCH_BOX_H_ 1


#include "ui/miniui/widget/RCMiniUIEditView.h"


BEGIN_NAMESPACE_RC

class SearchFrame;
class SearchBox;

class SearchEditDelegate : public RCMiniUIEditViewDelegate {
 public:
  SearchEditDelegate(SearchBox* parent);
  ~SearchEditDelegate() override;
  
  // RCMiniUIEditViewDelegate override
  bool OnEnChanged() override;
 private:
  SearchBox* parent_;
};

class SearchBox {
 public:
  SearchBox();
  ~SearchBox();

  void Init(SearchFrame* frame);
  void PrepareSearch();
  void SetSearchString(const std::wstring& search_string);
  std::wstring GetSearchString();

 private:
  SearchFrame* frame_;
  RCMiniUIEditView* edit_view_;
};

END_NAMESPACE_RC

#endif  // SEARCH_SEARCH_BOX_H_