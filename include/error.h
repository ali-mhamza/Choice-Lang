#pragma once
#include "token.h"
#include <string>
#include <string_view>

class LexError
{
	private:
		char errorChar;
		ui16 line;
		ui8 position;
		std::string_view message;

	public:
		LexError() = default;
		LexError(
			char c,
			ui16 line,
			ui8 position,
			std::string_view message
		);

		void report() const;
};

class CompileError
{
	private:
		Token token;
		std::string message;

	public:
		CompileError(const Token& token, const std::string& message);

		void report() const;
};

class RuntimeError
{
	private:
		Token token; // Temporarily, at least.
		std::string message;

	public:
		RuntimeError(const Token& token, const std::string& message);

		void report() const;
};

class TypeError; // For static type-checking.
class CodeError; // For invalid externally-loaded byte-code.