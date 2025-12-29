#include "../include/utils.h"

// Credit for ends_with and starts_with: Pavel P.
// Source: https://stackoverflow.com/questions/874134/.

bool ends_with(std::string_view str, std::string_view suffix)
{
    return (str.size() >= suffix.size()
			&& (str.compare(str.size() - suffix.size(), 
			suffix.size(), suffix) == 0));
}

bool starts_with(std::string_view str, std::string_view prefix)
{
    return (str.size() >= prefix.size()
			&& (str.compare(0, prefix.size(), prefix) == 0));
}