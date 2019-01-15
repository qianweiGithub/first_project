// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 1999-2016, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*
* File unistr.cpp
*
* Modification History:
*
*   Date        Name        Description
*   09/25/98    stephen     Creation.
*   04/20/99    stephen     Overhauled per 4/16 code review.
*   07/09/99    stephen     Renamed {hi,lo},{byte,word} to icu_X for HP/UX
*   11/18/99    aliu        Added handleReplaceBetween() to make inherit from
*                           Replaceable.
*   06/25/01    grhoten     Removed the dependency on iostream
******************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/appendable.h"
#include "unicode/putil.h"
#include "cstring.h"
#include "cmemory.h"
#include "unicode/ustring.h"
#include "unicode/unistr.h"
#include "unicode/utf.h"
#include "unicode/utf16.h"
#include "uelement.h"
#include "ustr_imp.h"
#include "umutex.h"
#include "uassert.h"

#if 0

#include <iostream>
using namespace std;

//DEBUGGING
void
print(const UnicodeString& s,
      const char *name)
{
  UChar c;
  cout << name << ":|";
  for(int i = 0; i < s.length(); ++i) {
    c = s[i];
    if(c>= 0x007E || c < 0x0020)
      cout << "[0x" << hex << s[i] << "]";
    else
      cout << (char) s[i];
  }
  cout << '|' << endl;
}

void
print(const UChar *s,
      int32_t len,
      const char *name)
{
  UChar c;
  cout << name << ":|";
  for(int i = 0; i < len; ++i) {
    c = s[i];
    if(c>= 0x007E || c < 0x0020)
      cout << "[0x" << hex << s[i] << "]";
    else
      cout << (char) s[i];
  }
  cout << '|' << endl;
}
// END DEBUGGING
#endif

// Local function definitions for now

// need to copy areas that may overlap
static
inline void
us_arrayCopy(const UChar *src, int32_t srcStart,
         UChar *dst, int32_t dstStart, int32_t count)
{
  if(count>0) {
    uprv_memmove(dst+dstStart, src+srcStart, (size_t)count*sizeof(*src));
  }
}

// u_unescapeAt() callback to get a UChar from a UnicodeString
U_CDECL_BEGIN
static UChar U_CALLCONV
UnicodeString_charAt(int32_t offset, void *context) {
    return ((icu::UnicodeString*) context)->charAt(offset);
}
U_CDECL_END

U_NAMESPACE_BEGIN

/* The Replaceable virtual destructor can't be defined in the header
   due to how AIX works with multiple definitions of virtual functions.
*/
Replaceable::~Replaceable() {}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(UnicodeString)

UnicodeString U_EXPORT2
operator+ (const UnicodeString &s1, const UnicodeString &s2) {
    return
        UnicodeString(s1.length()+s2.length()+1, (UChar32)0, 0).
            append(s1).
                append(s2);
}

//========================================
// Reference Counting functions, put at top of file so that optimizing compilers
//                               have a chance to automatically inline.
//========================================

void
UnicodeString::addRef() {
  umtx_atomic_inc((u_atomic_int32_t *)fUnion.fFields.fArray - 1);
}

int32_t
UnicodeString::removeRef() {
  return umtx_atomic_dec((u_atomic_int32_t *)fUnion.fFields.fArray - 1);
}

int32_t
UnicodeString::refCount() const {
  return umtx_loadAcquire(*((u_atomic_int32_t *)fUnion.fFields.fArray - 1));
}

void
UnicodeString::releaseArray() {
  if((fUnion.fFields.fLengthAndFlags & kRefCounted) && removeRef() == 0) {
    uprv_free((int32_t *)fUnion.fFields.fArray - 1);
  }
}



//========================================
// Constructors
//========================================

// The default constructor is inline in unistr.h.

UnicodeString::UnicodeString(int32_t capacity, UChar32 c, int32_t count) {
  fUnion.fFields.fLengthAndFlags = 0;
  if(count <= 0 || (uint32_t)c > 0x10ffff) {
    // just allocate and do not do anything else
    allocate(capacity);
  } else if(c <= 0xffff) {
    int32_t length = count;
    if(capacity < length) {
      capacity = length;
    }
    if(allocate(capacity)) {
      UChar *array = getArrayStart();
      UChar unit = (UChar)c;
      for(int32_t i = 0; i < length; ++i) {
        array[i] = unit;
      }
      setLength(length);
    }
  } else {  // supplementary code point, write surrogate pairs
    if(count > (INT32_MAX / 2)) {
      // We would get more than 2G UChars.
      allocate(capacity);
      return;
    }
    int32_t length = count * 2;
    if(capacity < length) {
      capacity = length;
    }
    if(allocate(capacity)) {
      UChar *array = getArrayStart();
      UChar lead = U16_LEAD(c);
      UChar trail = U16_TRAIL(c);
      for(int32_t i = 0; i < length; i += 2) {
        array[i] = lead;
        array[i + 1] = trail;
      }
      setLength(length);
    }
  }
}

UnicodeString::UnicodeString(UChar ch) {
  fUnion.fFields.fLengthAndFlags = kLength1 | kShortString;
  fUnion.fStackFields.fBuffer[0] = ch;
}

UnicodeString::UnicodeString(UChar32 ch) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  int32_t i = 0;
  UBool isError = FALSE;
  U16_APPEND(fUnion.fStackFields.fBuffer, i, US_STACKBUF_SIZE, ch, isError);
  // We test isError so that the compiler does not complain that we don't.
  // If isError then i==0 which is what we want anyway.
  if(!isError) {
    setShortLength(i);
  }
}

UnicodeString::UnicodeString(const UChar *text) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  doAppend(text, 0, -1);
}

UnicodeString::UnicodeString(const UChar *text,
                             int32_t textLength) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  doAppend(text, 0, textLength);
}

UnicodeString::UnicodeString(UBool isTerminated,
                             const UChar *text,
                             int32_t textLength) {
  fUnion.fFields.fLengthAndFlags = kReadonlyAlias;
  if(text == NULL) {
    // treat as an empty string, do not alias
    setToEmpty();
  } else if(textLength < -1 ||
            (textLength == -1 && !isTerminated) ||
            (textLength >= 0 && isTerminated && text[textLength] != 0)
  ) {
    setToBogus();
  } else {
    if(textLength == -1) {
      // text is terminated, or else it would have failed the above test
      textLength = u_strlen(text);
    }
    setArray((UChar *)text, textLength, isTerminated ? textLength + 1 : textLength);
  }
}

UnicodeString::UnicodeString(UChar *buff,
                             int32_t buffLength,
                             int32_t buffCapacity) {
  fUnion.fFields.fLengthAndFlags = kWritableAlias;
  if(buff == NULL) {
    // treat as an empty string, do not alias
    setToEmpty();
  } else if(buffLength < -1 || buffCapacity < 0 || buffLength > buffCapacity) {
    setToBogus();
  } else {
    if(buffLength == -1) {
      // fLength = u_strlen(buff); but do not look beyond buffCapacity
      const UChar *p = buff, *limit = buff + buffCapacity;
      while(p != limit && *p != 0) {
        ++p;
      }
      buffLength = (int32_t)(p - buff);
    }
    setArray(buff, buffLength, buffCapacity);
  }
}

UnicodeString::UnicodeString(const char *src, int32_t length, EInvariant) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  if(src==NULL) {
    // treat as an empty string
  } else {
    if(length<0) {
      length=(int32_t)uprv_strlen(src);
    }
    if(cloneArrayIfNeeded(length, length, FALSE)) {
      u_charsToUChars(src, getArrayStart(), length);
      setLength(length);
    } else {
      setToBogus();
    }
  }
}

#if U_CHARSET_IS_UTF8

UnicodeString::UnicodeString(const char *codepageData) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  if(codepageData != 0) {
    setToUTF8(codepageData);
  }
}

UnicodeString::UnicodeString(const char *codepageData, int32_t dataLength) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  // if there's nothing to convert, do nothing
  if(codepageData == 0 || dataLength == 0 || dataLength < -1) {
    return;
  }
  if(dataLength == -1) {
    dataLength = (int32_t)uprv_strlen(codepageData);
  }
  setToUTF8(StringPiece(codepageData, dataLength));
}

// else see unistr_cnv.cpp
#endif

UnicodeString::UnicodeString(const UnicodeString& that) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  copyFrom(that);
}

#if U_HAVE_RVALUE_REFERENCES
UnicodeString::UnicodeString(UnicodeString &&src) U_NOEXCEPT {
  fUnion.fFields.fLengthAndFlags = kShortString;
  moveFrom(src);
}
#endif

UnicodeString::UnicodeString(const UnicodeString& that,
                             int32_t srcStart) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  setTo(that, srcStart);
}

UnicodeString::UnicodeString(const UnicodeString& that,
                             int32_t srcStart,
                             int32_t srcLength) {
  fUnion.fFields.fLengthAndFlags = kShortString;
  setTo(that, srcStart, srcLength);
}

// Replaceable base class clone() default implementation, does not clone
Replaceable *
Replaceable::clone() const {
  return NULL;
}

// UnicodeString overrides clone() with a real implementation
Replaceable *
UnicodeString::clone() const {
  return new UnicodeString(*this);
}

//========================================
// array allocation
//========================================

namespace {

const int32_t kGrowSize = 128;

// The number of bytes for one int32_t reference counter and capacity UChars
// must fit into a 32-bit size_t (at least when on a 32-bit platform).
// We also add one for the NUL terminator, to avoid reallocation in getTerminatedBuffer(),
// and round up to a multiple of 16 bytes.
// This means that capacity must be at most (0xfffffff0 - 4) / 2 - 1 = 0x7ffffff5.
// (With more complicated checks we could go up to 0x7ffffffd without rounding up,
// but that does not seem worth it.)
const int32_t kMaxCapacity = 0x7ffffff5;

int32_t getGrowCapacity(int32_t newLength) {
  int32_t growSize = (newLength >> 2) + kGrowSize;
  if(growSize <= (kMaxCapacity - newLength)) {
    return newLength + growSize;
  } else {
    return kMaxCapacity;
  }
}

}  // namespace

UBool
UnicodeString::allocate(int32_t capacity) {
  if(capacity <= US_STACKBUF_SIZE) {
    fUnion.fFields.fLengthAndFlags = kShortString;
    return TRUE;
  }
  if(capacity <= kMaxCapacity) {
    ++capacity;  // for the NUL
    // Switch to size_t which is unsigned so that we can allocate up to 4GB.
    // Reference counter + UChars.
    size_t numBytes = sizeof(int32_t) + (size_t)capacity * U_SIZEOF_UCHAR;
    // Round up to a multiple of 16.
    numBytes = (numBytes + 15) & ~15;
    int32_t *array = (int32_t *) uprv_malloc(numBytes);
    if(array != NULL) {
      // set initial refCount and point behind the refCount
      *array++ = 1;
      numBytes -= sizeof(int32_t);

      // have fArray point to the first UChar
      fUnion.fFields.fArray = (UChar *)array;
      fUnion.fFields.fCapacity = (int32_t)(numBytes / U_SIZEOF_UCHAR);
      fUnion.fFields.fLengthAndFlags = kLongString;
      return TRUE;
    }
  }
  fUnion.fFields.fLengthAndFlags = kIsBogus;
  fUnion.fFields.fArray = 0;
  fUnion.fFields.fCapacity = 0;
  return FALSE;
}

////========================================
//// Destructor
////========================================
//
//#ifdef UNISTR_COUNT_FINAL_STRING_LENGTHS
//static u_atomic_int32_t finalLengthCounts[0x400];  // UnicodeString::kMaxShortLength+1
//static u_atomic_int32_t beyondCount(0);
//
//U_CAPI void unistr_printLengths() {
//  int32_t i;
//  for(i = 0; i <= 59; ++i) {
//    printf("%2d,  %9d\n", i, (int32_t)finalLengthCounts[i]);
//  }
//  int32_t beyond = beyondCount;
//  for(; i < UPRV_LENGTHOF(finalLengthCounts); ++i) {
//    beyond += finalLengthCounts[i];
//  }
//  printf(">59, %9d\n", beyond);
//}
//#endif

UnicodeString::~UnicodeString()
{
#ifdef UNISTR_COUNT_FINAL_STRING_LENGTHS
  // Count lengths of strings at the end of their lifetime.
  // Useful for discussion of a desirable stack buffer size.
  // Count the contents length, not the optional NUL terminator nor further capacity.
  // Ignore open-buffer strings and strings which alias external storage.
  if((fUnion.fFields.fLengthAndFlags&(kOpenGetBuffer|kReadonlyAlias|kWritableAlias)) == 0) {
    if(hasShortLength()) {
      umtx_atomic_inc(finalLengthCounts + getShortLength());
    } else {
      umtx_atomic_inc(&beyondCount);
    }
  }
#endif

  releaseArray();
}

////========================================
//// Factory methods
////========================================
//
//UnicodeString UnicodeString::fromUTF8(StringPiece utf8) {
//  UnicodeString result;
//  result.setToUTF8(utf8);
//  return result;
//}
//
//UnicodeString UnicodeString::fromUTF32(const UChar32 *utf32, int32_t length) {
//  UnicodeString result;
//  int32_t capacity;
//  // Most UTF-32 strings will be BMP-only and result in a same-length
//  // UTF-16 string. We overestimate the capacity just slightly,
//  // just in case there are a few supplementary characters.
//  if(length <= US_STACKBUF_SIZE) {
//    capacity = US_STACKBUF_SIZE;
//  } else {
//    capacity = length + (length >> 4) + 4;
//  }
//  do {
//    UChar *utf16 = result.getBuffer(capacity);
//    int32_t length16;
//    UErrorCode errorCode = U_ZERO_ERROR;
//    u_strFromUTF32WithSub(utf16, result.getCapacity(), &length16,
//        utf32, length,
//        0xfffd,  // Substitution character.
//        NULL,    // Don't care about number of substitutions.
//        &errorCode);
//    result.releaseBuffer(length16);
//    if(errorCode == U_BUFFER_OVERFLOW_ERROR) {
//      capacity = length16 + 1;  // +1 for the terminating NUL.
//      continue;
//    } else if(U_FAILURE(errorCode)) {
//      result.setToBogus();
//    }
//    break;
//  } while(TRUE);
//  return result;
//}
//
//========================================
// Assignment
//========================================

UnicodeString &
UnicodeString::operator=(const UnicodeString &src) {
  return copyFrom(src);
}

UnicodeString &
UnicodeString::copyFrom(const UnicodeString &src, UBool fastCopy) {
  // if assigning to ourselves, do nothing
  if(this == &src) {
    return *this;
  }

  // is the right side bogus?
  if(src.isBogus()) {
    setToBogus();
    return *this;
  }

  // delete the current contents
  releaseArray();

  if(src.isEmpty()) {
    // empty string - use the stack buffer
    setToEmpty();
    return *this;
  }

  // fLength>0 and not an "open" src.getBuffer(minCapacity)
  fUnion.fFields.fLengthAndFlags = src.fUnion.fFields.fLengthAndFlags;
  switch(src.fUnion.fFields.fLengthAndFlags & kAllStorageFlags) {
  case kShortString:
    // short string using the stack buffer, do the same
    uprv_memcpy(fUnion.fStackFields.fBuffer, src.fUnion.fStackFields.fBuffer,
                getShortLength() * U_SIZEOF_UCHAR);
    break;
  case kLongString:
    // src uses a refCounted string buffer, use that buffer with refCount
    // src is const, use a cast - we don't actually change it
    ((UnicodeString &)src).addRef();
    // copy all fields, share the reference-counted buffer
    fUnion.fFields.fArray = src.fUnion.fFields.fArray;
    fUnion.fFields.fCapacity = src.fUnion.fFields.fCapacity;
    if(!hasShortLength()) {
      fUnion.fFields.fLength = src.fUnion.fFields.fLength;
    }
    break;
  case kReadonlyAlias:
    if(fastCopy) {
      // src is a readonly alias, do the same
      // -> maintain the readonly alias as such
      fUnion.fFields.fArray = src.fUnion.fFields.fArray;
      fUnion.fFields.fCapacity = src.fUnion.fFields.fCapacity;
      if(!hasShortLength()) {
        fUnion.fFields.fLength = src.fUnion.fFields.fLength;
      }
      break;
    }
    // else if(!fastCopy) fall through to case kWritableAlias
    // -> allocate a new buffer and copy the contents
    U_FALLTHROUGH;
  case kWritableAlias: {
    // src is a writable alias; we make a copy of that instead
    int32_t srcLength = src.length();
    if(allocate(srcLength)) {
      u_memcpy(getArrayStart(), src.getArrayStart(), srcLength);
      setLength(srcLength);
      break;
    }
    // if there is not enough memory, then fall through to setting to bogus
    U_FALLTHROUGH;
  }
  default:
    // if src is bogus, set ourselves to bogus
    // do not call setToBogus() here because fArray and flags are not consistent here
    fUnion.fFields.fLengthAndFlags = kIsBogus;
    fUnion.fFields.fArray = 0;
    fUnion.fFields.fCapacity = 0;
    break;
  }

  return *this;
}

UnicodeString &UnicodeString::moveFrom(UnicodeString &src) U_NOEXCEPT {
  // No explicit check for self move assignment, consistent with standard library.
  // Self move assignment causes no crash nor leak but might make the object bogus.
  releaseArray();
  copyFieldsFrom(src, TRUE);
  return *this;
}

// Same as moveFrom() except without memory management.
void UnicodeString::copyFieldsFrom(UnicodeString &src, UBool setSrcToBogus) U_NOEXCEPT {
  int16_t lengthAndFlags = fUnion.fFields.fLengthAndFlags = src.fUnion.fFields.fLengthAndFlags;
  if(lengthAndFlags & kUsingStackBuffer) {
    // Short string using the stack buffer, copy the contents.
    // Check for self assignment to prevent "overlap in memcpy" warnings,
    // although it should be harmless to copy a buffer to itself exactly.
    if(this != &src) {
      uprv_memcpy(fUnion.fStackFields.fBuffer, src.fUnion.fStackFields.fBuffer,
                  getShortLength() * U_SIZEOF_UCHAR);
    }
  } else {
    // In all other cases, copy all fields.
    fUnion.fFields.fArray = src.fUnion.fFields.fArray;
    fUnion.fFields.fCapacity = src.fUnion.fFields.fCapacity;
    if(!hasShortLength()) {
      fUnion.fFields.fLength = src.fUnion.fFields.fLength;
    }
    if(setSrcToBogus) {
      // Set src to bogus without releasing any memory.
      src.fUnion.fFields.fLengthAndFlags = kIsBogus;
      src.fUnion.fFields.fArray = NULL;
      src.fUnion.fFields.fCapacity = 0;
    }
  }
}

//========================================
// Read-only implementation
//========================================
UBool
UnicodeString::doEquals(const UnicodeString &text, int32_t len) const {
  // Requires: this & text not bogus and have same lengths.
  // Byte-wise comparison works for equality regardless of endianness.
  return uprv_memcmp(getArrayStart(), text.getArrayStart(), len * U_SIZEOF_UCHAR) == 0;
}

int8_t
UnicodeString::doCompare( int32_t start,
              int32_t length,
              const UChar *srcChars,
              int32_t srcStart,
              int32_t srcLength) const
{
  // compare illegal string values
  if(isBogus()) {
    return -1;
  }
  
  // pin indices to legal values
  pinIndices(start, length);

  if(srcChars == NULL) {
    // treat const UChar *srcChars==NULL as an empty string
    return length == 0 ? 0 : 1;
  }

  // get the correct pointer
  const UChar *chars = getArrayStart();

  chars += start;
  srcChars += srcStart;

  int32_t minLength;
  int8_t lengthResult;

  // get the srcLength if necessary
  if(srcLength < 0) {
    srcLength = u_strlen(srcChars + srcStart);
  }

  // are we comparing different lengths?
  if(length != srcLength) {
    if(length < srcLength) {
      minLength = length;
      lengthResult = -1;
    } else {
      minLength = srcLength;
      lengthResult = 1;
    }
  } else {
    minLength = length;
    lengthResult = 0;
  }

  /*
   * note that uprv_memcmp() returns an int but we return an int8_t;
   * we need to take care not to truncate the result -
   * one way to do this is to right-shift the value to
   * move the sign bit into the lower 8 bits and making sure that this
   * does not become 0 itself
   */

  if(minLength > 0 && chars != srcChars) {
    int32_t result;

#   if U_IS_BIG_ENDIAN 
      // big-endian: byte comparison works
      result = uprv_memcmp(chars, srcChars, minLength * sizeof(UChar));
      if(result != 0) {
        return (int8_t)(result >> 15 | 1);
      }
#   else
      // little-endian: compare UChar units
      do {
        result = ((int32_t)*(chars++) - (int32_t)*(srcChars++));
        if(result != 0) {
          return (int8_t)(result >> 15 | 1);
        }
      } while(--minLength > 0);
#   endif
  }
  return lengthResult;
}

int32_t
UnicodeString::getLength() const {
    return length();
}

UChar
UnicodeString::getCharAt(int32_t offset) const {
  return charAt(offset);
}

UChar32
UnicodeString::getChar32At(int32_t offset) const {
  return char32At(offset);
}

UChar32
UnicodeString::char32At(int32_t offset) const
{
  int32_t len = length();
  if((uint32_t)offset < (uint32_t)len) {
    const UChar *array = getArrayStart();
    UChar32 c;
    U16_GET(array, 0, offset, len, c);
    return c;
  } else {
    return kInvalidUChar;
  }
}

void
UnicodeString::doExtract(int32_t start,
             int32_t length,
             UChar *dst,
             int32_t dstStart) const
{
  // pin indices to legal values
  pinIndices(start, length);

  // do not copy anything if we alias dst itself
  const UChar *array = getArrayStart();
  if(array + start != dst + dstStart) {
    us_arrayCopy(array, start, dst, dstStart, length);
  }
}

int32_t
UnicodeString::extract(UChar *dest, int32_t destCapacity,
                       UErrorCode &errorCode) const {
  int32_t len = length();
  if(U_SUCCESS(errorCode)) {
    if(isBogus() || destCapacity<0 || (destCapacity>0 && dest==0)) {
      errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    } else {
      const UChar *array = getArrayStart();
      if(len>0 && len<=destCapacity && array!=dest) {
        u_memcpy(dest, array, len);
      }
      return u_terminateUChars(dest, destCapacity, len, &errorCode);
    }
  }

  return len;
}

int32_t
UnicodeString::extract(int32_t start,
                       int32_t length,
                       char *target,
                       int32_t targetCapacity,
                       enum EInvariant) const
{
  // if the arguments are illegal, then do nothing
  if(targetCapacity < 0 || (targetCapacity > 0 && target == NULL)) {
    return 0;
  }

  // pin the indices to legal values
  pinIndices(start, length);

  if(length <= targetCapacity) {
    u_UCharsToChars(getArrayStart() + start, target, length);
  }
  UErrorCode status = U_ZERO_ERROR;
  return u_terminateChars(target, targetCapacity, length, &status);
}

UnicodeString
UnicodeString::tempSubString(int32_t start, int32_t len) const {
  pinIndices(start, len);
  const UChar *array = getBuffer();  // not getArrayStart() to check kIsBogus & kOpenGetBuffer
  if(array==NULL) {
    array=fUnion.fStackFields.fBuffer;  // anything not NULL because that would make an empty string
    len=-2;  // bogus result string
  }
  return UnicodeString(FALSE, array + start, len);
}

void 
UnicodeString::extractBetween(int32_t start,
                  int32_t limit,
                  UnicodeString& target) const {
  pinIndex(start);
  pinIndex(limit);
  doExtract(start, limit - start, target);
}

void
UnicodeString::setToBogus()
{
  releaseArray();

  fUnion.fFields.fLengthAndFlags = kIsBogus;
  fUnion.fFields.fArray = 0;
  fUnion.fFields.fCapacity = 0;
}

// turn a bogus string into an empty one
void
UnicodeString::unBogus() {
  if(fUnion.fFields.fLengthAndFlags & kIsBogus) {
    setToEmpty();
  }
}

// setTo() analogous to the readonly-aliasing constructor with the same signature
UnicodeString &
UnicodeString::setTo(UBool isTerminated,
                     const UChar *text,
                     int32_t textLength)
{
  if(fUnion.fFields.fLengthAndFlags & kOpenGetBuffer) {
    // do not modify a string that has an "open" getBuffer(minCapacity)
    return *this;
  }

  if(text == NULL) {
    // treat as an empty string, do not alias
    releaseArray();
    setToEmpty();
    return *this;
  }

  if( textLength < -1 ||
      (textLength == -1 && !isTerminated) ||
      (textLength >= 0 && isTerminated && text[textLength] != 0)
  ) {
    setToBogus();
    return *this;
  }

  releaseArray();

  if(textLength == -1) {
    // text is terminated, or else it would have failed the above test
    textLength = u_strlen(text);
  }
  fUnion.fFields.fLengthAndFlags = kReadonlyAlias;
  setArray((UChar *)text, textLength, isTerminated ? textLength + 1 : textLength);
  return *this;
}

UnicodeString&
UnicodeString::append(UChar32 srcChar) {
  UChar buffer[U16_MAX_LENGTH];
  int32_t _length = 0;
  UBool isError = FALSE;
  U16_APPEND(buffer, _length, U16_MAX_LENGTH, srcChar, isError);
  // We test isError so that the compiler does not complain that we don't.
  // If isError then _length==0 which turns the doAppend() into a no-op anyway.
  return isError ? *this : doAppend(buffer, 0, _length);
}

UnicodeString&
UnicodeString::doReplace( int32_t start,
              int32_t length,
              const UnicodeString& src,
              int32_t srcStart,
              int32_t srcLength)
{
  // pin the indices to legal values
  src.pinIndices(srcStart, srcLength);

  // get the characters from src
  // and replace the range in ourselves with them
  return doReplace(start, length, src.getArrayStart(), srcStart, srcLength);
}

UnicodeString&
UnicodeString::doReplace(int32_t start,
             int32_t length,
             const UChar *srcChars,
             int32_t srcStart,
             int32_t srcLength)
{
  if(!isWritable()) {
    return *this;
  }

  int32_t oldLength = this->length();

  // optimize (read-only alias).remove(0, start) and .remove(start, end)
  if((fUnion.fFields.fLengthAndFlags&kBufferIsReadonly) && srcLength == 0) {
    if(start == 0) {
      // remove prefix by adjusting the array pointer
      pinIndex(length);
      fUnion.fFields.fArray += length;
      fUnion.fFields.fCapacity -= length;
      setLength(oldLength - length);
      return *this;
    } else {
      pinIndex(start);
      if(length >= (oldLength - start)) {
        // remove suffix by reducing the length (like truncate())
        setLength(start);
        fUnion.fFields.fCapacity = start;  // not NUL-terminated any more
        return *this;
      }
    }
  }

  if(start == oldLength) {
    return doAppend(srcChars, srcStart, srcLength);
  }

  if(srcChars == 0) {
    srcStart = srcLength = 0;
  } else if(srcLength < 0) {
    // get the srcLength if necessary
    srcLength = u_strlen(srcChars + srcStart);
  }

  // pin the indices to legal values
  pinIndices(start, length);

  // Calculate the size of the string after the replace.
  // Avoid int32_t overflow.
  int32_t newLength = oldLength - length;
  if(srcLength > (INT32_MAX - newLength)) {
    setToBogus();
    return *this;
  }
  newLength += srcLength;

  // cloneArrayIfNeeded(doCopyArray=FALSE) may change fArray but will not copy the current contents;
  // therefore we need to keep the current fArray
  UChar oldStackBuffer[US_STACKBUF_SIZE];
  UChar *oldArray;
  if((fUnion.fFields.fLengthAndFlags&kUsingStackBuffer) && (newLength > US_STACKBUF_SIZE)) {
    // copy the stack buffer contents because it will be overwritten with
    // fUnion.fFields values
    u_memcpy(oldStackBuffer, fUnion.fStackFields.fBuffer, oldLength);
    oldArray = oldStackBuffer;
  } else {
    oldArray = getArrayStart();
  }

  // clone our array and allocate a bigger array if needed
  int32_t *bufferToDelete = 0;
  if(!cloneArrayIfNeeded(newLength, getGrowCapacity(newLength),
                         FALSE, &bufferToDelete)
  ) {
    return *this;
  }

  // now do the replace

  UChar *newArray = getArrayStart();
  if(newArray != oldArray) {
    // if fArray changed, then we need to copy everything except what will change
    us_arrayCopy(oldArray, 0, newArray, 0, start);
    us_arrayCopy(oldArray, start + length,
                 newArray, start + srcLength,
                 oldLength - (start + length));
  } else if(length != srcLength) {
    // fArray did not change; copy only the portion that isn't changing, leaving a hole
    us_arrayCopy(oldArray, start + length,
                 newArray, start + srcLength,
                 oldLength - (start + length));
  }

  // now fill in the hole with the new string
  us_arrayCopy(srcChars, srcStart, newArray, start, srcLength);

  setLength(newLength);

  // delayed delete in case srcChars == fArray when we started, and
  // to keep oldArray alive for the above operations
  if (bufferToDelete) {
    uprv_free(bufferToDelete);
  }

  return *this;
}

// Versions of doReplace() only for append() variants.
// doReplace() and doAppend() optimize for different cases.

UnicodeString&
UnicodeString::doAppend(const UnicodeString& src, int32_t srcStart, int32_t srcLength) {
  if(srcLength == 0) {
    return *this;
  }

  // pin the indices to legal values
  src.pinIndices(srcStart, srcLength);
  return doAppend(src.getArrayStart(), srcStart, srcLength);
}

UnicodeString&
UnicodeString::doAppend(const UChar *srcChars, int32_t srcStart, int32_t srcLength) {
  if(!isWritable() || srcLength == 0 || srcChars == NULL) {
    return *this;
  }

  if(srcLength < 0) {
    // get the srcLength if necessary
    if((srcLength = u_strlen(srcChars + srcStart)) == 0) {
      return *this;
    }
  }

  int32_t oldLength = length();
  int32_t newLength = oldLength + srcLength;
  // optimize append() onto a large-enough, owned string
  if((newLength <= getCapacity() && isBufferWritable()) ||
      cloneArrayIfNeeded(newLength, getGrowCapacity(newLength))) {
    UChar *newArray = getArrayStart();
    // Do not copy characters when
    //   UChar *buffer=str.getAppendBuffer(...);
    // is followed by
    //   str.append(buffer, length);
    // or
    //   str.appendString(buffer, length)
    // or similar.
    if(srcChars + srcStart != newArray + oldLength) {
      us_arrayCopy(srcChars, srcStart, newArray, oldLength, srcLength);
    }
    setLength(newLength);
  }
  return *this;
}

/**
 * Replaceable API
 */
void
UnicodeString::handleReplaceBetween(int32_t start,
                                    int32_t limit,
                                    const UnicodeString& text) {
    replaceBetween(start, limit, text);
}

/**
 * Replaceable API
 */
void 
UnicodeString::copy(int32_t start, int32_t limit, int32_t dest) {
    if (limit <= start) {
        return; // Nothing to do; avoid bogus malloc call
    }
    UChar* text = (UChar*) uprv_malloc( sizeof(UChar) * (limit - start) );
    // Check to make sure text is not null.
    if (text != NULL) {
	    extractBetween(start, limit, text, 0);
	    insert(dest, text, 0, limit - start);    
	    uprv_free(text);
    }
}

/**
 * Replaceable API
 *
 * NOTE: This is for the Replaceable class.  There is no rep.cpp,
 * so we implement this function here.
 */
UBool Replaceable::hasMetaData() const {
    return TRUE;
}

/**
 * Replaceable API
 */
UBool UnicodeString::hasMetaData() const {
    return FALSE;
}

//========================================
// External Buffer
//========================================

UChar *
UnicodeString::getBuffer(int32_t minCapacity) {
  if(minCapacity>=-1 && cloneArrayIfNeeded(minCapacity)) {
    fUnion.fFields.fLengthAndFlags|=kOpenGetBuffer;
    setZeroLength();
    return getArrayStart();
  } else {
    return 0;
  }
}

void
UnicodeString::releaseBuffer(int32_t newLength) {
  if(fUnion.fFields.fLengthAndFlags&kOpenGetBuffer && newLength>=-1) {
    // set the new fLength
    int32_t capacity=getCapacity();
    if(newLength==-1) {
      // the new length is the string length, capped by fCapacity
      const UChar *array=getArrayStart(), *p=array, *limit=array+capacity;
      while(p<limit && *p!=0) {
        ++p;
      }
      newLength=(int32_t)(p-array);
    } else if(newLength>capacity) {
      newLength=capacity;
    }
    setLength(newLength);
    fUnion.fFields.fLengthAndFlags&=~kOpenGetBuffer;
  }
}

//========================================
// Miscellaneous
//========================================
UBool
UnicodeString::cloneArrayIfNeeded(int32_t newCapacity,
                                  int32_t growCapacity,
                                  UBool doCopyArray,
                                  int32_t **pBufferToDelete,
                                  UBool forceClone) {
  // default parameters need to be static, therefore
  // the defaults are -1 to have convenience defaults
  if(newCapacity == -1) {
    newCapacity = getCapacity();
  }

  // while a getBuffer(minCapacity) is "open",
  // prevent any modifications of the string by returning FALSE here
  // if the string is bogus, then only an assignment or similar can revive it
  if(!isWritable()) {
    return FALSE;
  }

  /*
   * We need to make a copy of the array if
   * the buffer is read-only, or
   * the buffer is refCounted (shared), and refCount>1, or
   * the buffer is too small.
   * Return FALSE if memory could not be allocated.
   */
  if(forceClone ||
     fUnion.fFields.fLengthAndFlags & kBufferIsReadonly ||
     (fUnion.fFields.fLengthAndFlags & kRefCounted && refCount() > 1) ||
     newCapacity > getCapacity()
  ) {
    // check growCapacity for default value and use of the stack buffer
    if(growCapacity < 0) {
      growCapacity = newCapacity;
    } else if(newCapacity <= US_STACKBUF_SIZE && growCapacity > US_STACKBUF_SIZE) {
      growCapacity = US_STACKBUF_SIZE;
    }

    // save old values
    UChar oldStackBuffer[US_STACKBUF_SIZE];
    UChar *oldArray;
    int32_t oldLength = length();
    int16_t flags = fUnion.fFields.fLengthAndFlags;

    if(flags&kUsingStackBuffer) {
      U_ASSERT(!(flags&kRefCounted)); /* kRefCounted and kUsingStackBuffer are mutally exclusive */
      if(doCopyArray && growCapacity > US_STACKBUF_SIZE) {
        // copy the stack buffer contents because it will be overwritten with
        // fUnion.fFields values
        us_arrayCopy(fUnion.fStackFields.fBuffer, 0, oldStackBuffer, 0, oldLength);
        oldArray = oldStackBuffer;
      } else {
        oldArray = NULL; // no need to copy from the stack buffer to itself
      }
    } else {
      oldArray = fUnion.fFields.fArray;
      U_ASSERT(oldArray!=NULL); /* when stack buffer is not used, oldArray must have a non-NULL reference */
    }

    // allocate a new array
    if(allocate(growCapacity) ||
       (newCapacity < growCapacity && allocate(newCapacity))
    ) {
      if(doCopyArray) {
        // copy the contents
        // do not copy more than what fits - it may be smaller than before
        int32_t minLength = oldLength;
        newCapacity = getCapacity();
        if(newCapacity < minLength) {
          minLength = newCapacity;
        }
        if(oldArray != NULL) {
          us_arrayCopy(oldArray, 0, getArrayStart(), 0, minLength);
        }
        setLength(minLength);
      } else {
        setZeroLength();
      }

      // release the old array
      if(flags & kRefCounted) {
        // the array is refCounted; decrement and release if 0
        u_atomic_int32_t *pRefCount = ((u_atomic_int32_t *)oldArray - 1);
        if(umtx_atomic_dec(pRefCount) == 0) {
          if(pBufferToDelete == 0) {
              // Note: cast to (void *) is needed with MSVC, where u_atomic_int32_t
              // is defined as volatile. (Volatile has useful non-standard behavior
              //   with this compiler.)
            uprv_free((void *)pRefCount);
          } else {
            // the caller requested to delete it himself
            *pBufferToDelete = (int32_t *)pRefCount;
          }
        }
      }
    } else {
      // not enough memory for growCapacity and not even for the smaller newCapacity
      // reset the old values for setToBogus() to release the array
      if(!(flags&kUsingStackBuffer)) {
        fUnion.fFields.fArray = oldArray;
      }
      fUnion.fFields.fLengthAndFlags = flags;
      setToBogus();
      return FALSE;
    }
  }
  return TRUE;
}

U_NAMESPACE_END

U_NAMESPACE_USE

// Moved here from uhash_us.cpp so that using a UVector of UnicodeString*
// does not depend on hashtable code.
U_CAPI UBool U_EXPORT2
uhash_compareUnicodeString(const UElement key1, const UElement key2) {
    const UnicodeString *str1 = (const UnicodeString*) key1.pointer;
    const UnicodeString *str2 = (const UnicodeString*) key2.pointer;
    if (str1 == str2) {
        return TRUE;
    }
    if (str1 == NULL || str2 == NULL) {
        return FALSE;
    }
    return *str1 == *str2;
}

#ifdef U_STATIC_IMPLEMENTATION
/*
This should never be called. It is defined here to make sure that the
virtual vector deleting destructor is defined within unistr.cpp.
The vector deleting destructor is already a part of UObject,
but defining it here makes sure that it is included with this object file.
This makes sure that static library dependencies are kept to a minimum.
*/
static void uprv_UnicodeStringDummy(void) {
    delete [] (new UnicodeString[2]);
}
#endif
