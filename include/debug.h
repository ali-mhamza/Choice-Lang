#pragma once
#include "astnodes.h"
#include "common.h"
#include <vector>

struct DebugRange
{
    bool isStmt{};
    AST::Statement::StmtType stmtType{};
    AST::Expression::ExprType exprType{};
    // The first instruction for the expression/statement.
    u64 byteStart{};
    // The last instruction for the expression/statement (not included).
    u64 byteEnd{};
    // Offset into the source code where the expression/statement starts.
    u64 sourceStart{};
    // Offset into the source code where the expression/statement ends
    // (not included; just past the end).
    u64 sourceEnd{};

    bool operator==(const DebugRange& other) const;
    bool bytesEqual(const DebugRange& other) const;
    bool locationEqual(const DebugRange& other) const;
    bool bytesInside(const DebugRange& other) const;
    bool locationInside(const DebugRange& other) const;
};

// The three cases for debug info with respect to a ByteCode cache file.
enum DebugInfoState : u8
{
    DEBUG_COMBINED, DEBUG_SEPARATE, DEBUG_STRIPPED
};

using DebugMetadata = std::vector<DebugRange>;