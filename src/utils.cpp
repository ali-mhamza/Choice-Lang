#include "../include/utils.h"
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

static std::vector<std::string_view>
split(const std::string_view& str, std::string_view delim)
{
    std::vector<std::string_view> result;
    size_t start = 0;

    for (size_t found = str.find(delim);
		found != std::string_view::npos;
		found = str.find(delim, start))
    {
		result.emplace_back(str.begin() + start, found - start);
        start = found + delim.size();
    }
    if (start != str.size())
        result.emplace_back(str.begin() + start, str.size() - start);
    return result;      
}

std::array<i32, 3> constructRange(std::string_view tokText)
{
	auto parts = split(tokText, "..");
	auto transform = [](const std::string_view& text) -> i32 {
		i32 ret = 0;
		for (char c : text)
		{
			if (isdigit(c))
				ret = (ret * 10) + (c - '0');
			else if (c != '\'')
				break;
		}
		
		return ret;
	};

	std::array<i32, 3> nums;
	nums[0] = transform(parts[0]);
	nums[1] = transform(parts[1]);
	if (parts.size() == 3)
		nums[2] = transform(parts[2]);
	else
		nums[2] = 1;
	return nums;
}