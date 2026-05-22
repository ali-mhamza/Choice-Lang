#pragma once
#include "common.h"
#include "diagnostic.h"
#include "object.h"
#include "opcodes.h"
#include "token.h"
#include <string>
#include <string_view>

struct Error
{
   	DiagCode code{};
	const std::string label{};
};

struct RuntimeError : public Error
{
	RuntimeError(DiagCode code, const std::string& label = "");
};

struct TypeMismatch : public Error {};

TypeMismatch
reportBinaryOperator(Opcode oper, const Object& first, const Object& second);
TypeMismatch
reportUnaryOperator(Opcode oper, const Object& obj);
TypeMismatch
reportCollection(DiagCode code, const Object& first, const Object& second = Object{});

class TypeError; // For static type-checking.
class CodeError; // For invalid externally-loaded byte-code.