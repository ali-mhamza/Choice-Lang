#include <iostream>
#include <type_traits>

enum class LogKind
{
	A,
	B,
	C,
	COUNT
};

enum class ShowKind
{
	A,
	B,
	C,
	COUNT
};

#define EachEnumVal(type, var)														\
	(																				\
		type var{static_cast<type>(0)};												\
		var < type::COUNT;															\
		var = static_cast<type>(static_cast<std::underlying_type_t<type>>(var) + 1)	\
	)

int main()
{
	for EachEnumVal(LogKind, kind)
		std::cout << (int) kind << '\n';
	for EachEnumVal(ShowKind, kind)
		std::cout << (int) kind << '\n';
}