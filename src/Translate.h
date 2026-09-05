#pragma once

#include <string>

// X() args must be plain identifiers (MSVC preprocessor quirk with `##`).
#define TR_KEYS(X)                                                            \
    X(REVIVE,         "Revive")                                   \
    X(UNUSED_ENTRY, "UNUSED_ENTRY")

enum TrKey {
#define X(n, s) TR_##n,
    TR_KEYS(X)
#undef X
    TR_COUNT
};

const char* Tr(TrKey key);
void DetectLanguage();
