#ifndef SEARCH_SCOPED_GDIPLUS_INITIALIZER_H_
#define SEARCH_SCOPED_GDIPLUS_INITIALIZER_H_ 1


class ScopedGdiplusInitializer {
 public:
  ScopedGdiplusInitializer();
  ~ScopedGdiplusInitializer();

 private:
  unsigned long gdiplus_token_;
};  // class ScopedGdiplusInitializer


#endif  // BASE_SCOPED_GDIPLUS_INITIALIZER_H_
