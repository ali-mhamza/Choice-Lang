#pragma once
#include "token.h"
#include <string>
#include <string_view>

class LexError
{
	private:
		const char errorChar{};
		const ui16 line{};
		const ui8 position{};
		const std::string_view message{};

	public:
		LexError() = default;
		LexError(
			const char c,
			const ui16 line,
			const ui8 position,
			const std::string_view message
		);

		void report() const;
};

class CompileError
{
	private:
		const Token token{};
		const std::string message{};

	public:
		CompileError(const Token& token, const std::string& message);

		void report() const;
};

class RuntimeError
{
	private:
		const Token token{}; // Temporarily, at least.
		const std::string message{};

	public:
		RuntimeError(const Token& token, const std::string& message);

		void report() const;
};

class TypeError; // For static type-checking.
class CodeError; // For invalid externally-loaded byte-code.