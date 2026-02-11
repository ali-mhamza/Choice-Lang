#pragma once
#include "token.h"
#include <string>
#include <string_view>

class Error
{
	protected:
		std::string message;

	public:
		Error() = default;
		Error(const std::string& message);
};

class LexError
{
	private:
		char errorChar;
		ui16 line;
		ui8 position;
		std::string_view message;

	public:
		LexError() = default;
		LexError(char c, ui16 line, ui8 position,
			std::string_view message);

		void report() const;
};

class CompileError : public Error
{
	private:
		Token token;

	public:
		CompileError(const Token& token, const std::string& message);

		void report() const;
};

class RuntimeError : public Error
{
	private:
		Token token; // Temporarily, at least.

	public:
		RuntimeError(const Token& token, const std::string& message);

		void report() const;
};

class TypeError : public Error {}; // For static type-checking.
class CodeError : public Error {}; // For invalid externally-loaded byte-code.