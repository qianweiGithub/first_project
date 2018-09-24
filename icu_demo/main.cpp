#include "unicode/uidna.h"
#include "unicode/ptypes.h"

int main(){
    UErrorCode code = U_ZERO_ERROR;
    uint32_t options = 0;
    UIDNA* uidna = uidna_openUTS46(options, &code);
    UChar* name = nullptr;
    UChar* dest = nullptr;
    int32_t capacity = 0;
    UIDNAInfo*  info = nullptr;
    uidna_nameToASCII(uidna, name, 0, dest, capacity, info, &code);
}