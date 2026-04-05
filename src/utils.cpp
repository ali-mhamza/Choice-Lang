#include "../include/utils.h"
#include "../include/common.h"
#include <cstddef> 		// For size_t.
#include <string_view>
#include <vector>

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

// Partial credit for split: Shubham Agrawal.
// Source: https://stackoverflow.com/questions/14265581/.
std::vector<std::string_view> split(
	const std::string_view& str,
	std::string_view delim
)
{
    std::vector<std::string_view> result{};
    size_t start{0};

    for (size_t found{str.find(delim)};
		found != std::string_view::npos;
		found = str.find(delim, start))
    {
		result.emplace_back(str.data() + start, found - start);
        start = found + delim.size();
    }
    if (start != str.size())
        result.emplace_back(str.data() + start, str.size() - start);
    return result;      
}