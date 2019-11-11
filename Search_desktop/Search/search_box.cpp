#include "Search/search_box.h"

#include "Search/search_frame.h"

BEGIN_NAMESPACE_RC

SearchEditDelegate::SearchEditDelegate(SearchBox* parent)
    :parent_(parent) {
}

SearchEditDelegate::~SearchEditDelegate() {
}

bool SearchEditDelegate::OnEnChanged() {
  parent_->PrepareSearch();

  return false;
}

SearchBox::SearchBox()
    : frame_(nullptr),
      edit_view_(nullptr) {
}

SearchBox::~SearchBox() {
}

void SearchBox::Init(SearchFrame* frame) {
  frame_ = frame;
  edit_view_ = (RCMiniUIEditView*)frame_->GetViewByID(IDC_FRAME_WINDOW_EDIT_SEARCH);
  edit_view_->SetDelegate(new SearchEditDelegate(this));
}

void SearchBox::PrepareSearch() {
  frame_->KillWindowTimer(SEARCH_DELAY_TIMER_ID);
  frame_->SetWindowTimer(SEARCH_DELAY_TIMER_ID, 100);
}

void SearchBox::SetSearchString(const std::wstring& search_string) {
  edit_view_->SetInnerText(search_string, true);
}

std::wstring SearchBox::GetSearchString() {
  return edit_view_->GetInnerText();
}

END_NAMESPACE_RC