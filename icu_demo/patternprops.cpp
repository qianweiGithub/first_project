// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  patternprops.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011mar13
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "patternprops.h"

U_NAMESPACE_BEGIN

/*
 * One byte per Latin-1 character.
 * Bit 0 is set if either Pattern property is true,
 * bit 1 if Pattern_Syntax is true,
 * bit 2 if Pattern_White_Space is true.
 * That is, Pattern_Syntax is encoded as 3 and Pattern_White_Space as 5.
 */
static const uint8_t latin1[256]={
    // WS: 9..D
    0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // WS: 20  Syntax: 21..2F
    5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    // Syntax: 3A..40
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3,
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Syntax: 5B..5E
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 0,
    // Syntax: 60
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Syntax: 7B..7E
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 0,
    // WS: 85
    0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Syntax: A1..A7, A9, AB, AC, AE
    0, 3, 3, 3, 3, 3, 3, 3, 0, 3, 0, 3, 3, 0, 3, 0,
    // Syntax: B0, B1, B6, BB, BF
    3, 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, 0, 0, 3,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Syntax: D7
    0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Syntax: F7
    0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0
};

UBool
PatternProps::isWhiteSpace(UChar32 c) {
    if(c<0) {
        return FALSE;
    } else if(c<=0xff) {
        return (UBool)(latin1[c]>>2)&1;
    } else if(0x200e<=c && c<=0x2029) {
        return c<=0x200f || 0x2028<=c;
    } else {
        return FALSE;
    }
}

const UChar *
PatternProps::skipWhiteSpace(const UChar *s, int32_t length) {
    while(length>0 && isWhiteSpace(*s)) {
        ++s;
        --length;
    }
    return s;
}

U_NAMESPACE_END
