#pragma once
#include "common.h"
#include "diagnostic.h"
#include "object.h"
#include "opcodes.h"
#include "token.h"
#include <string>
#include <string_view>

struct RuntimeError
{
	DiagCode code{};
	const std::string label{};

	RuntimeError(DiagCode code, const std::string& label = "");
};

RuntimeError
reportBinaryOperator(Opcode oper, const Object& first, const Object& second);
RuntimeError
reportUnaryOperator(Opcode oper, const Object& obj);
RuntimeError
reportCollection(DiagCode code, const Object& first, const Object& second = Object{});

class TypeError; // For static type-checking.
class CodeError; // For invalid externally-loaded byte-code.