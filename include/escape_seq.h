#pragma once

#include "token.h"
#include <string>
#include <string_view>

using svIter    = std::string_view::const_iterator;

bool parseNumericSequence(
    std::string& str,
    svIter& it,
    svIter end,
    std::string& errorMsg
);
bool parseCharSequence(std::string& str, svIter& it);