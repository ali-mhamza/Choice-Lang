#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

namespace Object
{
	enum ObjType : uint8_t
	{
		OBJ_BASE,
		OBJ_INT,
		OBJ_UINT,
		OBJ_DEC,
		OBJ_BOOL,
		OBJ_STRING,
		OBJ_NULL,
		OBJ_INVALID
	};
	
	struct Base
	{   
		ObjType type;   // Object type.
		size_t size;    // Object size.

		Base();
		virtual ~Base() = default;
		Base(ObjType type, size_t size);

		virtual std::string print() = 0;
		virtual std::string printType();
		virtual void emit(std::ofstream& is);
	};

	using BaseUP = std::unique_ptr<Base>;
	
	template<typename SizeInt>
	struct Int : public Base
	{
		SizeInt value;

		Int();
		Int(SizeInt value);

		std::string print() override;
		std::string printType() override;
		void emit(std::ofstream& is) override;
	};

	using IntType = std::variant<int8_t, int16_t, int32_t, int64_t>;
	using IntVariant = Int<IntType>;

	template<typename SizeUInt>
	struct UInt : public Base
	{
		SizeUInt value;

		UInt();
		UInt(SizeUInt value);

		std::string print() override;
		std::string printType() override;
		void emit(std::ofstream& is) override;
	};

	using UIntType = std::variant<uint8_t, uint16_t, uint32_t, uint64_t>;
	using UIntVariant = UInt<UIntType>;

	template<typename SizeDec>
	struct Dec : public Base
	{
		SizeDec value;

		Dec();
		Dec(SizeDec value);

		std::string print() override;
		std::string printType() override;
		void emit(std::ofstream& is) override;
	};

	using DecType = std::variant<float, double>;
	using DecVariant = Dec<DecType>;

	struct Bool : public Base
	{
		bool value;

		Bool() = default;
		Bool(bool value);

		std::string print() override;
		std::string printType() override;
	};

	struct String : public Base
	{
		std::string_view value;
		std::string alt;

		String();
		String(std::string_view& value);

		// Factory method.
		static String makeString(const std::string& value);
		std::string print() override;
		std::string printType() override;
		void emit(std::ofstream& is) override;
	};

	struct Null : public Base
	{
		Null();

		std::string print() override;
		std::string printType() override;
	};
};

using namespace Object;
#define IS_NUM(type) \
		((type == OBJ_INT) || (type == OBJ_UINT) \
		|| (type == OBJ_DEC))

// Int.

template<typename SizeInt>
Int<SizeInt>::Int() :
	Base(OBJ_INT, sizeof(SizeInt)), value(0) {}

template<typename SizeInt>
Int<SizeInt>::Int(SizeInt value) :
	Base(OBJ_INT, sizeof(SizeInt)), value(value) {}

template<typename SizeInt>
std::string Int<SizeInt>::print()
{
	std::ostringstream oss;
	if (this->size != 1)
		oss << this->value;
	else
		oss << static_cast<int>(this->value);
	return oss.str();
}

template<typename SizeInt>
std::string Int<SizeInt>::printType()
{
	std::ostringstream oss;
	oss << "INT";
	switch (this->size)
	{
		case 1: oss << "8";     break;
		case 2: oss << "16";    break;
		case 4: oss << "32";    break;
		case 8: oss << "64";    break;
	}
	return oss.str();
}

template<typename SizeInt>
void Int<SizeInt>::emit(std::ofstream& os)
{
	os.put(static_cast<char>(OBJ_INT));
	os.put(static_cast<char>(size));
	Int<SizeInt> temp(*this);
	for (size_t i = 0; i < size; i++)
		os.put(static_cast<char>(temp.value >> (size - i - 1)));
}

// UInt.
template<typename SizeUInt>
UInt<SizeUInt>::UInt() :
	Base(OBJ_INT, sizeof(SizeUInt)), value(0) {}

template<typename SizeUInt>
UInt<SizeUInt>::UInt(SizeUInt value) :
	Base(OBJ_UINT, sizeof(SizeUInt)), value(value) {}

template<typename SizeUInt>
std::string UInt<SizeUInt>::print()
{
	std::ostringstream oss;
	if (this->size != 1)
		oss << this->value;
	else
		oss << static_cast<int>(this->value);
	return oss.str();
}

template<typename SizeUInt>
std::string UInt<SizeUInt>::printType()
{
	std::ostringstream oss;
	oss << "UINT";
	switch (this->size)
	{
		case 1: oss << "8";     break;
		case 2: oss << "16";    break;
		case 4: oss << "32";    break;
		case 8: oss << "64";    break;
	}
	return oss.str();
}

template<typename SizeUInt>
void UInt<SizeUInt>::emit(std::ofstream& os)
{
	os.put(static_cast<char>(OBJ_UINT));
	os.put(static_cast<char>(size));
	UInt<SizeUInt> temp(*this);
	for (size_t i = 0; i < size; i++)
		os.put(static_cast<char>(temp.value >> (size - i - 1)));
}

// Dec.

template<typename SizeDec>
Dec<SizeDec>::Dec() :
	Base(OBJ_DEC, sizeof(SizeDec)), value(0.0) {}

// template<typename SizeDec>
// Dec<SizeDec>::Dec() :
//     Base(OBJ_DEC, sizeof(SizeDec))
// {
//     if constexpr (std::is_same_v<SizeDec, float>)
//         value = 0.0f;
//     else if constexpr (std::is_same_v<SizeDec, double>)
//         value = 0.0;
// }

template<typename SizeDec>
Dec<SizeDec>::Dec(SizeDec value) :
	Base(OBJ_DEC, sizeof(SizeDec)), value(value) {}

template<typename SizeDec>
std::string Dec<SizeDec>::print()
{
	std::ostringstream oss;
	oss << this->value;
	return oss.str();
}

template<typename SizeDec>
std::string Dec<SizeDec>::printType()
{
	std::ostringstream oss;
	oss << "DEC";
	switch (this->size)
	{
		case sizeof(float): oss << "32";	break;
		case sizeof(double): oss << "64";	break;
	}
	return oss.str();
}

template<typename SizeDec>
void Dec<SizeDec>::emit(std::ofstream& os)
{
	os.put(static_cast<char>(OBJ_DEC));
	os.put(static_cast<char>(size));

	char bytes[sizeof(SizeDec)];
	std::memcpy(&bytes[0], &value, size);
	for (size_t i = 0; i < size; i++)
		os.put(bytes[i]);
}