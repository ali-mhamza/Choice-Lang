/*
 * General utility functions to handle files, normalize or format
 * strings, as well as perform string checks.
 */

#include "../include/utils.h"
#include "../include/config.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

std::ifstream openFile(
	const std::filesystem::path& filePath,
	bool binary,
	const std::string_view message
)
{
	std::ifstream fileStream{};

	if (binary)
	    fileStream.open(filePath, std::ios::binary);
	else
        fileStream.open(filePath);

	if (fileStream.fail() || !fileStream.is_open())
	{
		CH_PRINT_ERROR_ARGS("{}\n", message);
		exit(66);
	}

	return fileStream;
}

std::string readFile(std::ifstream& stream)
{
    std::stringstream buffer{};
	buffer << stream.rdbuf();
	std::string fileString{buffer.str()};
	stream.close();
	return fileString;
}

std::string readFile(const std::filesystem::path& filePath, bool binary)
{
	std::ifstream fileStream{openFile(filePath, binary)};
	return readFile(fileStream);
}

bool fileMoreRecent(
    const std::filesystem::path& a,
    const std::filesystem::path& b
)
{
	using std::filesystem::exists;
	using std::filesystem::last_write_time;

	return (exists(a) && exists(b)
			&& (last_write_time(a) >= last_write_time(b)));
}

void normalizeInput(std::string& input)
{
	input.erase(std::remove_if(input.begin(), input.end(),
	[](char c) -> bool {
		return (isspace(c) && (c != ' ')
				&& (c != '\n') && (c != '\t'));
    }), input.end());

    auto it{input.find('\t')};
    while (it != input.npos)
    {
		// Normalize all tabs with spaces.
        input.replace(it, 1, std::string(TAB_SIZE, ' '));
        it = input.find('\t', it + 1);
    }
}

std::string formatMultiLineString(const std::string_view& sv)
{
    std::string newStr{sv};

    auto it{newStr.find('\n')};
    while (it != newStr.npos)
    {
        newStr.replace(it, 1, CH_NEWLINE_REPLACEMENT);
        it = newStr.find('\n', it + 1);
    }

    return newStr;
}

// Credit for endsWith and startsWith: Pavel P.
// Source: https://stackoverflow.com/questions/874134/.

bool endsWith(std::string_view str, std::string_view suffix)
{
    return (str.size() >= suffix.size()
			&& (str.compare(str.size() - suffix.size(),
			suffix.size(), suffix) == 0));
}

bool startsWith(std::string_view str, std::string_view prefix)
{
    return (str.size() >= prefix.size()
			&& (str.compare(0, prefix.size(), prefix) == 0));
}

// TODO: Handle any encoding errors.
std::vector<std::string> getRunes(std::string_view str)
{
	#define PUSH_BACK_SEQ(count)			\
		do {								\
			for (u64 i{0}; i < count; i++)	\
			{								\
				entry.push_back(*it);		\
				it++;						\
			}								\
		} while (false)

	std::vector<std::string> array;
	auto it{str.begin()};
	while (it != str.end())
	{
		auto& entry{array.emplace_back()};
		char c{*it};
		if ((c & 0b11111000) == 0b11110000)
			PUSH_BACK_SEQ(4);
		else if ((c & 0b11110000) == 0b11100000)
			PUSH_BACK_SEQ(3);
		else if ((c & 0b11100000) == 0b11000000)
			PUSH_BACK_SEQ(2);
		else if ((c & 0b10000000) == 0b00000000)
			PUSH_BACK_SEQ(1);
		else
			CH_ASSERT(false, "Invalid byte value in UTF-8 string.");
	}

	return array;

	#undef PUSH_BACK_SEQ
}

// Partial credit for split: Shubham Agrawal.
// Source: https://stackoverflow.com/questions/14265581/.
std::vector<std::string> split(std::string_view str, std::string_view delim)
{
    std::vector<std::string> result{};
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