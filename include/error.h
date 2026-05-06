#pragma once
#include "token.h"
#include <string>
#include <string_view>

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