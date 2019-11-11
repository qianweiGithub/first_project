#ifndef PUBLIC_MACROS_H_
#define PUBLIC_MACROS_H_ 1

#include <stddef.h>  // For size_t


#define DISALLOW_COPY(TypeName) \
  TypeName(const TypeName&) = delete

#define DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&) = delete

// 以下两个宏必须放在类的私有声明中
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

template<typename T, size_t N> char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

#define COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)

template <class Dest, class Source>
inline Dest bit_cast(const Source& source) {
  COMPILE_ASSERT(sizeof(Dest) == sizeof(Source), VerifySizesAreEqual);

  Dest dest;
  ::memcpy(&dest, &source, sizeof(dest));
  return dest;
}


#define PATH_BUFFER_SIZE (2048)

#endif  // PUBLIC_MACROS_H_
