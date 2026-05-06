#pragma once

#include "diagnostic.h"
#include <string>
#include <string_view>
#include <utility>

using svIter = std::string_view::const_iterator;
using ErrorPair = std::pair<DiagCode, std::string>;

[[nodiscard]] bool parseNumericSequence(
    std::string& str,
    svIter& it,
    const svIter end,
    ErrorPair& pair
);

[[nodiscard]] bool parseUnicodeSequence(
    std::string& str,
    svIter& it,
    const svIter end,
    ErrorPair& pair
);

[[nodiscard]] bool parseCharSequence(
    std::string& str,
    svIter& it,
    const svIter end
);