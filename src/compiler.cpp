/*
 * All code for the compiler in the interpreter pipeline.
 * At global scope, the compiler takes an AST and returns
 * a ByteCode object (with associated metadata).
 */

#include "../include/compiler.h"
#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/constructors.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/escape_seq.h"
#include "../include/linear_alloc.h"
#include "../include/natives.h"
#include "../include/object.h"
#include "../include/opcodes.h"
#include "../include/token.h"
#include "../include/utils.h"
#include <personal/hash_table.h>
#include <algorithm>
#include <climits>
#include <string_view>
#include <utility>
#include <vector>

using namespace AST::Statement;
using namespace AST::Expression;

/* General macros. */

#define DEF(type) void Compiler::compile##type(const type* node)
#define COMPILE(type)                                   \
    do {                                                \
        auto* ptr = static_cast<type*>(node.get());     \
        compile##type(ptr);                             \
    } while (false)

#define REPORT_ERROR(...)           \
    do {                            \
        reportError(__VA_ARGS__);   \
        return;                     \
    } while (false)

constexpr bool accessFix{false};
constexpr bool accessVar{true};

constexpr bool getVar{true};
constexpr bool setVar{false};

/* Constructors/destructors. */

Compiler::Compiler(Compiler* comp) :
    scopeCompiler{comp},
    depth{static_cast<u8>(comp == nullptr ? 0 : comp->depth + 1)}
{
    if (depth == 0) // Global scope compiler.
        defineBuiltinGlobals();
    else
        this->id = scopeCompiler->id;
}

Compiler::~Compiler() = default;

bool Compiler::clearDeclaredVars{false};
u8 Compiler::clearIndex{0};

void Compiler::defineBuiltinGlobals()
{
    defVar("_file_", nextReg++, accessFix);
    for (const auto* func : Constructors::ctorNames)
        defVar(func, nextReg++, accessFix); // For now.
    for (const auto* func : Natives::funcNames)
        defVar(func, nextReg++, accessVar);
}

void Compiler::defineBuiltinLocals(const std::string& funcName)
{
    defVar("_func_", nextReg, accessFix);
    Object nameObj{};
    if (funcName.empty()) // Lambda.
        nameObj = CH_ALLOC(String, "lambda");
    else
        nameObj = CH_ALLOC(String, funcName);

    MAKE_FIXED(nameObj);
    MAKE_IMMUT(nameObj);
    code.loadRegConst(nameObj, nextReg++);
}

/* Compilation helpers. */

void Compiler::emitVariableOp(bool type, const VarInfo& info, u8 dest, u8 src)
{
    if (info.type == GLOBAL)
    {
        code.addOp((type == getVar ? OP_GET_GLOBAL : OP_SET_GLOBAL),
            dest, src);
    }
    else if (info.type == LOCAL)
    {
        code.addOp((type == getVar ? OP_GET_LOCAL : OP_SET_LOCAL),
            dest, src);
    }
    else
    {
        code.addOp((type == getVar ? OP_GET_CELL : OP_SET_CELL),
            dest, src);
    }
}

void Compiler::emitUnpackState(const AST::UnpackState& unpack)
{
    code.addByte(static_cast<u8>(unpack.unpackLastVar));
    code.addByte(static_cast<u8>(unpack.unpackIgnore));
}

// We pass an std::string instead of std::string_view
// since the line containing the variable's text will
// likely be destroyed soon after (if using the REPL),
// and thus we must take ownership of the string first
// to avoid invalidating the view.
void Compiler::defVar(const std::string& name, u8 reg, bool access)
{
    (*varLocations)[{ name, scope }] = reg;
    (*varAccess)[reg] = access;

    if (scope != 0)
        varScopes.top().push_back(name);
    else if (inRepl && (depth == 0) && (scope == 0))
        declaredVars.push_back({ name, reg });
}

void Compiler::removeVar(const std::string& name)
{
    VarEntry entry{name, scope};
    u8 reg{(*varLocations)[entry]};
    varLocations->remove(entry);
    varAccess->remove(reg);

    if (scope != 0)
        varScopes.top().pop_back();
    else if (inRepl && (depth == 0) && (scope == 0))
        declaredVars.pop_back();
}

void Compiler::clearDeclarations()
{
    if (clearDeclaredVars)
    {
        CH_ASSERT(clearIndex < declaredVars.size(),
            "Incorrect value for clearIndex field.");
        nextReg = declaredVars[clearIndex].reg;
        while (clearIndex < declaredVars.size())
        {
            const auto& pair{declaredVars[clearIndex++]};

            varLocations->remove({ pair.name, scope });
            varAccess->remove(pair.reg);
        }
    }

    declaredVars.clear();
    clearDeclaredVars = false;
    clearIndex = 0;
}

bool Compiler::getAccess(u8 reg) const
{
    bool* ret{varAccess->get(reg)};
    CH_ASSERT(ret != nullptr,
        "Variable registered with no access field.");
    return *ret;
}

Compiler::LocalInfo Compiler::getScopeLocal(const Token& token) const
{
    VarEntry entry{token.text, scope};
    u8* slot{varLocations->get(entry)};
    if (slot != nullptr)
        return { true, *slot };

    return { false };
}

Compiler::VarInfo Compiler::resolveVariable(const Token& token)
{
    // Check if variable is local first.
    for (u8 i{0}; i <= scope; i++)
    {
        VarEntry entry{token.text, static_cast<u8>(scope - i)};
        u8* slot{varLocations->get(entry)};
        if (slot != nullptr)
        {
            VarType type{LOCAL};
            if ((depth == 0) && (entry.scope == 0))
                type = GLOBAL;
            return { true, *slot, type, getAccess(*slot) };
        }
    }

    // Check enclosing non-global scopes.
    if (scopeCompiler != nullptr)
    {
        VarInfo info{scopeCompiler->resolveVariable(token)};
        if (info.found)
        {
            info.inCell = (info.type == CELL);
            info.slot = captureVariable(token, info);
            // Local variables in enclosing scopes become cells in
            // the current scope.
            if (info.type == LOCAL) info.type = CELL;
            return info;
        }
    }

    return { false };
}

u8 Compiler::captureVariable(const Token& token, const VarInfo& info)
{
    if (info.type == GLOBAL)
        return info.slot;

    std::string name{token.text};
    u8* index{captureNames.get(name)};
    if (index != nullptr) // Already captured -> don't capture again.
        return *index;

    u8 cellIndex{static_cast<u8>(captures.size())};
    captureNames[name] = cellIndex;
    captures.push_back({ info.slot, info.inCell });
    return cellIndex;
}

void Compiler::pushScope()
{
    scope++;
    scopeStart = nextReg;
    varScopes.emplace();
    code.addOp(OP_ENTER_SCOPE, scopeStart);
}

void Compiler::popScope()
{
    auto& scopeVars{varScopes.top()};
    for (std::string& var : scopeVars)
        varLocations->remove({var, scope});

    varScopes.pop();
    scope--;
    nextReg = scopeStart;
    code.addOp(OP_EXIT_SCOPE);
}

void Compiler::startDeclaration()
{
    if (inRepl) code.addOp(OP_DEF_START, declaredVars.size());
}

void Compiler::endDeclaration()
{
    if (inRepl) code.addOp(OP_DEF_END);
}

void Compiler::patchLoopLabelJumps(const Token& label, bool patchBreaks)
{
    if (label.type == TOK_EOF) return;

    if (patchBreaks)
    {
        auto* vec{breakLabels->get(label.text)};
        for (u64 jump : *vec)
            code.patchJump(jump);
        // Breaks are always patched at the very end.
        breakLabels->remove(label.text);
        continueLabels->remove(label.text);
    }
    else
    {
        auto* vec{continueLabels->get(label.text)};
        for (u64 jump : *vec)
            code.patchJump(jump);
    }
}

/* String helper. */

std::string Compiler::parseStringToken(
    const Token& token,
    size_t start,
    size_t offset
)
{
    #define REPORT(CURRENT, OFF, LEN)                                   \
        reportPartError(pair.first, token,                              \
            start + static_cast<u64>((CURRENT) - text.begin() - (OFF)), \
            (LEN), pair.second                                          \
        )

	auto size{token.text.size() - offset};
    if (size == 0) return std::string{}; // Empty string.

	const auto text{token.text.substr(start, size)};
    auto it{text.begin()};
    auto end{text.end()};

    std::string str{};
	str.reserve(size);

    // Skip leading or trailing newlines.
    if (it[0] == '\n') it++;
    if (end[-1] == '\n') end--;

    bool reportedError{false};
    ErrorPair pair{std::make_pair(static_cast<DiagCode>(0), "")};
    decltype(it) current{};

    // Keep as inequality check in case 'end' becomes before 'it'.
	while (it < end)
	{
	    current = it;
		if ((*it == '\\') && (it < end - 1))
		{
            if (parseCharSequence(str, it, end)
                || parseNumericSequence(str, it, end, pair)
                || parseUnicodeSequence(str, it, end, pair))
            {
                continue;
            }
            else if (!reportedError && (static_cast<u8>(pair.first) != 0))
            {
                if (pair.first == HIT_OCTAL_CHAR_MAX)
                    REPORT(it, 3, 3);
                else if (pair.first == INVALID_UTF_CODEPOINT)
                    REPORT(current + 3, 0, it - (current + 3) - 1);
                else
                    REPORT(it, 0, 1);
                reportedError = true;
            }
		}

		str.push_back(*it);
        it++;
	}

    return str;

    #undef REPORT
}

/* Error reporting. */

void Compiler::reportError(
    DiagCode code,
    const Token& token,
    std::string_view message
)
{
    diagEngine.source = ErrorSource::COMPILER;
    hitError = true;
    if ((code == WRONG_TOKEN_FOUND) || (code == WRONG_CHAR_FOUND))
        code = (token.type == TOK_EOF) ? UNEXPECTED_INPUT_END : code;
    diagEngine.recordError(id, code, token, std::string{message});
}

void Compiler::reportPart(
    bool isError,
    DiagCode code,
    u64 offset,
    u64 length,
    std::string_view message
)
{
    diagEngine.source = ErrorSource::COMPILER;

    if (isError)
        diagEngine.recordError(id, code, offset, length, std::string{message});
    else
        diagEngine.recordWarning(id, code, offset, length, std::string{message});
}

void Compiler::reportPartError(
    DiagCode code,
    const Token& token,
    u64 offset,
    u64 length,
    std::string_view message
)
{
    hitError = true;
    if ((code == WRONG_TOKEN_FOUND) || (code == WRONG_CHAR_FOUND))
        code = (token.type == TOK_EOF) ? UNEXPECTED_INPUT_END : code;
    reportPart(true, code, token.byteOffset + offset, length, message);
}

/* AST node compilation functions. */

void Compiler::compileSingleVarDecl(
    const Token& name,
    bool fix,
    bool init,
    u8 valueReg
)
{
    LocalInfo localInfo{getScopeLocal(name)};

    if (localInfo.found)
    {
        if (inRepl && (depth == 0) && (scope == 0))
        {
            (*varAccess)[localInfo.slot] = (fix ? accessFix : accessVar);
            if (init)
            {
                // Always a local variable.
                code.addOp(OP_SET_LOCAL, localInfo.slot, valueReg);
            }
            else
                code.loadReg(localInfo.slot, OP_NULL);
        }
        else
            reportError(VAR_ALREADY_DEFINED, name);
        return;
    }

    std::string varName{name.text};
    u8 varSlot{valueReg};
    defVar(varName, varSlot, (fix ? accessFix : accessVar));

    if (!init) code.loadReg(varSlot, OP_NULL);
}

DEF(VarDecl)
{
    u8 nameCount{static_cast<u8>(node->names.size())};
    u8 valueCount{static_cast<u8>(node->values.size())};

    if (valueCount > 1)
    {
        if (nameCount > valueCount)
            REPORT_ERROR(UNPACK_TOO_FEW, node->oper);
        else if (!node->unpack && (nameCount < valueCount))
            REPORT_ERROR(UNPACK_TOO_MANY, node->oper);
    }

    // We signal the beginning of the declaration here so that
    // the failure of any initializer at runtime undefines all
    // of the variables declared here.
    startDeclaration();

    u8 valueStart{nextReg};
    if (valueCount != 0)
    {
        for (const auto& value : node->values)
            compileExpr(value);
        if (hitError)
        {
            endDeclaration(); // We don't define any variables if an error occurred.
            return;
        }
    }

    if ((nameCount > 1) && (valueCount == 1))
    {
        code.addOp(OP_UNPACK, valueStart, nameCount);
        emitUnpackState(node->unpack);
    }

    for (u64 i{0}; i < nameCount; i++)
    {
        compileSingleVarDecl(node->names[i], node->fix, (valueCount != 0),
            valueStart + i);
    }

    code.addOp((node->fix ? OP_FIX : OP_VAR), valueStart, nameCount);
    endDeclaration();

    // To make sure that variable registers are reserved,
    // regardless of runtime errors.
    nextReg = valueStart + nameCount;
}

std::pair<ByteCode*, u8> Compiler::paramHelper(
    Compiler& miniCompiler,
    const std::vector<AST::Param>& params
)
{
    ByteCode* defaultArgs{new ByteCode[params.size()]};
    ByteCode* temp{defaultArgs};
    // Temporary bytecode storage since we recycle the compiler's
    // bytecode.
    ByteCode store{};

    for (auto it{params.begin()}; it != params.end(); it++)
    {
        const Token& param{it->param};
        u8 reg{miniCompiler.nextReg};
        LocalInfo info{miniCompiler.getScopeLocal(param)};
        if (info.found)
            reportError(PARAM_ALREADY_DEFINED, param);

        // We evaluate the default value before binding the parameter
        // so parameters can't reference themselves.
        if (it->defaultVal != nullptr)
        {
            miniCompiler.compileExpr(it->defaultVal);
            *temp = miniCompiler.getCode();
            temp->addOp(OP_HALT);
            temp++;
            miniCompiler.code.clearCode();
        }
        else
        {
            if (it->variadic) store.addOp(OP_VAR_ARGS, reg);
            miniCompiler.reserveReg();
        }

        // Apply OP_FIX after (potential) OP_VAR_ARGS, since
        // the latter may overwrite flags in the type byte for
        // the fix-marked register.
        bool access{accessVar};
        if (it->fix)
        {
            store.addOp(OP_FIX, reg, u8(1));
            access = accessFix;
        }
        miniCompiler.defVar(std::string{param.text}, reg, access);
    }

    miniCompiler.code = store;
    return std::make_pair(defaultArgs, static_cast<u8>(temp - defaultArgs));
}

Object Compiler::makeFuncObj(
    Compiler& miniCompiler,
    const std::vector<AST::Param>& params,
    const StmtUP& body,
    const std::string& name
)
{
    auto [defaultArgs, defaultCount] = paramHelper(miniCompiler, params);
    miniCompiler.defineBuiltinLocals(name);

    miniCompiler.compileStmt(body);
    miniCompiler.code.addOp(OP_VOID, 0);
    miniCompiler.code.addOp(OP_RETURN, 0);

    ByteCode& funcCode{miniCompiler.getCode()};
    if (miniCompiler.hitError)
        this->hitError = true;

    Object func{};
    bool variadic{!params.empty() && params.back().variadic};
    u8 arity{static_cast<u8>(params.size() - (variadic ? 1 : 0))};

    if (name.empty()) // Compiling a lambda.
        func = CH_ALLOC(Function, funcCode, arity - defaultCount, arity);
    else
        func = CH_ALLOC(Function, name, funcCode, arity - defaultCount, arity);

    AS_USER_FUNC(func)->defaultArgs = defaultArgs;
    AS_USER_FUNC(func)->variadic = variadic;

    return func;
}

void Compiler::funcBodyHelper(
    Compiler& miniCompiler,
    const std::vector<AST::Param>& params,
    const StmtUP& body,
    const u8 funcReg,
    const std::string& name
)
{
    Object func{makeFuncObj(miniCompiler, params, body, name)};

    // We only declare in the current function scope.
    code.loadRegConst(func, funcReg);
    if (!miniCompiler.captures.empty())
        code.addOp(OP_CLOSURE, funcReg);

    for (const auto& info : miniCompiler.captures)
    {
        // Capture object in register [slot] from enclosing scope,
        // or reuse the cell at index [slot] from enclosing scope.
        code.addOp((info.inCell ? OP_CAPTURE_CELL : OP_CAPTURE_VAL),
            funcReg, info.slot);
    }
}

DEF(FuncDecl)
{
    startDeclaration();
    if (node->name.text.size() > CODE_MAX)
    {
        REPORT_ERROR(FUNC_NAME_TOO_LONG, node->name,
            "maximum length is 255 characters");
    }

    LocalInfo localInfo{getScopeLocal(node->name)};
    bool redefined{false};
    if (localInfo.found)
    {
        if (inRepl && (depth == 0) && (scope == 0))
            redefined = true;
        else
            REPORT_ERROR(FUNC_ALREADY_DEFINED, node->name);
    }

    if (node->params.size() > PARAMETER_MAX)
        REPORT_ERROR(HIT_PARAM_MAX, node->params[PARAMETER_MAX].param);

    u8 varSlot{redefined ? localInfo.slot : nextReg};
    std::string name{node->name.text};
    if (!redefined)
    {
        defVar(name, varSlot, accessVar);
        reserveReg();
    }

    bool inError{hitError};
    Compiler miniCompiler{this};
    funcBodyHelper(miniCompiler, node->params, node->body, varSlot, name);
    if (!inError && hitError) removeVar(name);
    endDeclaration();
}

// Using basic loops for the time being (assuming small types).

bool Compiler::checkFieldCollisions(
    const TypeDecl* node
)
{
    const auto& fields{node->fields};
    const u64 fieldCount{fields.size()};
    for (u64 i{0}; i < fieldCount; i++)
    {
        for (u64 j{0}; j < fieldCount; j++)
        {
            if (i == j) continue;

            if (fields[i].name.text == fields[j].name.text)
            {
                reportError(FIELD_ALREADY_DEFINED, fields[std::max(i, j)].name);
                return false;
            }
        }
    }

    return true;
}

bool Compiler::checkMethodCollisions(
    const TypeDecl* node
)
{
    const auto& methods{node->methods};
    const u64 methodCount{methods.size()};

    for (u64 i{0}; i < methodCount; i++)
    {
        const FuncDecl* firstDecl{static_cast<FuncDecl*>(methods[i].get())};
        for (u64 j{0}; j < methodCount; j++)
        {
            if (i == j) continue;

            const FuncDecl* secondDecl{static_cast<FuncDecl*>(methods[j].get())};
            if (firstDecl->name.text == secondDecl->name.text)
            {
                reportError(METHOD_ALREADY_DEFINED, secondDecl->name);
                return false;
            }
        }
    }

    return true;
}

bool Compiler::checkMixedCollisions(
    const TypeDecl* node
)
{
    const auto& fields{node->fields};
    const u64 fieldCount{fields.size()};

    const auto& methods{node->methods};
    const u64 methodCount{methods.size()};

    for (u64 i{0}; i < fieldCount; i++)
    {
        for (u64 j{0}; j < methodCount; j++)
        {
            const FuncDecl* decl{static_cast<FuncDecl*>(methods[j].get())};
            if (fields[i].name.text == decl->name.text)
            {
                reportError(METHOD_FIELD_COLLIDE, decl->name);
                return false;
            }
        }
    }

    return true;
}

bool Compiler::checkTypeNameCollisions(
    const TypeDecl* node
)
{
    return checkFieldCollisions(node) && checkMethodCollisions(node)
        && checkMixedCollisions(node);
}

DEF(TypeDecl)
{
    if (!checkTypeNameCollisions(node)) return;

    u8 typeReg{nextReg};
    std::string name{node->name.text};
    defVar(name, typeReg, accessVar);
    reserveReg();

    std::vector<Type::FieldPair> fields{};
    ByteCode* fieldInits{new ByteCode[node->fields.size()]};
    ByteCode* temp{fieldInits};

    for (const auto& field : node->fields)
    {
        fields.emplace_back(std::string{field.name.text}, field.fix);
        if (field.init != nullptr)
        {
            Compiler initCompiler{this};
            initCompiler.compileExpr(field.init);
            *temp = initCompiler.getCode();
        }
        else
            temp->loadReg(0, OP_NULL);

        temp->addOp(OP_HALT);
        temp++;
    }

    u8 methodStart{nextReg};
    for (const auto& method : node->methods)
    {
        FuncDecl* decl{static_cast<FuncDecl*>(method.get())};
        std::string name{decl->name.text};

        Compiler miniCompiler{this};
        u8 funcReg{nextReg};

        miniCompiler.defVar("self", 0, accessFix);
        miniCompiler.reserveReg();
        funcBodyHelper(miniCompiler, decl->params, decl->body, funcReg, name);
    }

    Object typeObj{CH_ALLOC(Type, name, fields, fieldInits)};
    code.loadRegConst(typeObj, typeReg);

    for (u64 i{0}; i < node->methods.size(); i++)
        code.addOp(OP_METHOD, typeReg, static_cast<u8>(methodStart + i));
    nextReg = typeReg + 1; // Methods shouldn't continue to live in registers.
}

DEF(UseStmt)
{
    std::string name{node->module.text};
    std::string dir{};
    std::string alias{};
    if (node->directory.type != TOK_EOF)
    {
        // Trim quote-marks around the path string as well.
        dir = std::string{node->directory.text.substr(1)};
        dir.pop_back();
    }
    if (node->alias.type != TOK_EOF)
        alias = std::string{node->alias.text};

    startDeclaration();
    Object module{CH_ALLOC(Module, name)};
    Object directory{CH_ALLOC(String, dir)};

    u8 moduleReg{nextReg};
    code.loadRegConst(module, moduleReg);
    defVar(alias.empty() ? name : alias, nextReg, accessVar);
    reserveReg();

    u8 directoryReg{nextReg};
    code.loadRegConst(directory, directoryReg);
    code.addOp(OP_MODULE, moduleReg, directoryReg);
    endDeclaration();
}

DEF(IfStmt)
{
    u8 reg{compileExpr(node->condition)};
    u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();
    compileStmt(node->trueBranch);
    if (node->falseBranch != nullptr)
    {
        u64 trueJump{code.addJump(OP_JUMP)};
        code.patchJump(falseJump);
        if (node->falseBranch != nullptr)
            compileStmt(node->falseBranch);
        code.patchJump(trueJump);
    }
    else
        code.patchJump(falseJump);
}

DEF(WhileStmt)
{
    u8 reg{nextReg};
    u64 loopStart{code.getLoopStart()};
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<u64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<u64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    compileExpr(node->condition);
    u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();
    compileStmt(node->body);

    // Patch current scope "continue" jumps.
    for (u64 jump : continues)
        code.patchJump(jump);

    // Patch nested scope "continue" jumps.
    patchLoopLabelJumps(node->label, false);
    code.addLoop(loopStart);

    code.patchJump(falseJump);
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

    // Patch current scope "break" jumps.
    for (u64 jump : breaks)
        code.patchJump(jump);

    // Patch nested scope "break" jumps.
    patchLoopLabelJumps(node->label, true);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

void Compiler::forLoopHelper(
    const ForStmt* node,
    const u8 varReg,
    const u8 iterReg
)
{
    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    u64 failJump{code.addJump(OP_JUMP)}; // If we fail to construct an iterator.
    u64 loopStart{code.getLoopStart()};

    u64 whereJump{0};
    if (node->header.where != nullptr)
    {
        u8 whereReg{compileExpr(node->header.where)};
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
        freeReg();
    }

    u8 varCount{static_cast<u8>(node->header.vars.size())};
    if (varCount > 1)
    {
        code.addOp(OP_UNPACK, varReg, varCount);
        emitUnpackState(node->header.unpack);
    }
    if (node->header.fix) code.addOp(OP_FIX, varReg, varCount);
    compileStmt(node->body);

    if (whereJump != 0)
        code.patchJump(whereJump);
    // Patch current scope "continue" jumps.
    for (u64 jump : *continueJumps)
        code.patchJump(jump);

    // Patch nested scope "continue" jumps.
    patchLoopLabelJumps(node->label, false);

    constexpr int UPDATE_ITER_OP_SIZE{5};
    u16 diff{static_cast<u16>(code.codeSize() - loopStart
        + UPDATE_ITER_OP_SIZE)};
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<u8>((diff >> CHAR_BIT) & CODE_MAX),
        static_cast<u8>(diff & CODE_MAX)
    );

    code.patchJump(failJump);
    compileStmt(node->elseClause); // Will do nothing if elseClause == nullptr.

    // Patch current scope "break" jumps.
    for (u64 jump : *breakJumps)
        code.patchJump(jump);

    // Patch nested scope "break" jumps.
    patchLoopLabelJumps(node->label, true);
}

DEF(ForStmt)
{
    pushScope();
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<u64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<u64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    u8 varReg{nextReg};
    bool fix{node->header.fix ? accessFix : accessVar};
    for (const auto& var : node->header.vars)
    {
        defVar(std::string{var.text}, nextReg, fix);
        reserveReg();
    }

    u8 iterReg{compileExpr(node->header.iter)};
    forLoopHelper(node, varReg, iterReg);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;

    popScope();
}

void Compiler::matchCaseHelper(
    const MatchStmt::MatchCase& checkCase,
    const u8 matchReg,
    u64& fallJump,
    u64& emptyJump
)
{
    u8 caseReg{compileExpr(checkCase.value)};
    code.addOp(OP_EQUAL, caseReg, matchReg);
    u64 falseJump{code.addJump(OP_JUMP_FALSE, caseReg)};
    freeReg();

    if (fallJump != 0) // We skip condition checking during fallthrough.
        code.patchJump(fallJump);
    if (emptyJump != 0)
    {
        code.patchJump(emptyJump);
        emptyJump = 0;
    }

    // We check here since compileStmt will call .release()
    // on the unique_ptr body field, which will make it a
    // nullptr regardless.
    bool empty = (checkCase.body == nullptr);
    compileStmt(checkCase.body); // Can handle empty (nullptr) body.

    // If we have fallthrough, or there's already fallthrough,
    // fall/keep falling.
    if (checkCase.fallthrough || (fallJump != 0))
        fallJump = code.addJump(OP_JUMP);
    else if (empty) // Default fallthrough with empty match blocks.
        emptyJump = code.addJump(OP_JUMP);
    else
        endJumps->push_back(code.addJump(OP_JUMP));

    code.patchJump(falseJump);
}

DEF(MatchStmt)
{
    u8 matchReg{compileExpr(node->matchValue)};
    std::vector<u64> jumps{};
    auto* prevEndJumps{endJumps};
    endJumps = &jumps;

    u64 fallJump{0}; // Invalid jump offset value.
    u64 emptyJump{0};

    for (const auto& checkCase : node->cases)
    {
        if (checkCase.value != nullptr)
            matchCaseHelper(checkCase, matchReg, fallJump, emptyJump);
        else // Default case.
            compileStmt(checkCase.body); // No need for any jumps.
    }

    for (u64 jump : jumps)
        code.patchJump(jump);
    freeReg(); // Remove the match value.

    endJumps = prevEndJumps;
}

DEF(RepeatStmt)
{
    if (node->label.type != TOK_EOF)
    {
        breakLabels->add(node->label.text, {});
        continueLabels->add(node->label.text, {});
    }

    std::vector<u64> breaks{};
    auto* prevBreaks{breakJumps};
    breakJumps = &breaks;

    std::vector<u64> continues{};
    auto* prevContinues{continueJumps};
    continueJumps = &continues;

    u64 loopStart{code.getLoopStart()};
    compileStmt(node->body);

    // Patch current scope "continue" jumps.
    for (u64 jump : continues)
        code.patchJump(jump);
    // Patch nested scope "continue" jumps.
    patchLoopLabelJumps(node->label, false);

    u8 reg{compileExpr(node->condition)};
    u64 trueJump{code.addJump(OP_JUMP_TRUE, reg)};
    freeReg();

    code.addLoop(loopStart);
    code.patchJump(trueJump);

    // Patch current scope "break" jumps.
    for (u64 jump : breaks)
        code.patchJump(jump);

    // Patch nested scope "break" jumps.
    patchLoopLabelJumps(node->label, true);

    breakJumps = prevBreaks;
    continueJumps = prevContinues;
}

DEF(ReturnStmt)
{
    u8 reg{nextReg};
    if (node->expr != nullptr)
        compileExpr(node->expr);
    else
        code.addOp(OP_VOID, reg); // Return void value as default.
    code.addOp(OP_RETURN, reg);

    if (node->expr != nullptr) freeReg();
}

DEF(BreakStmt)
{
    if (node->label.type == TOK_EOF)
        this->breakJumps->push_back(code.addJump(OP_JUMP));
    else
    {
        auto* vec{breakLabels->get(node->label.text)};
        if (vec == nullptr)
            REPORT_ERROR(INVALID_LOOP_LABEL, node->label);
        else
            vec->push_back(code.addJump(OP_JUMP));
    }
}

DEF(ContinueStmt)
{
    if (node->label.type == TOK_EOF)
        this->continueJumps->push_back(code.addJump(OP_JUMP));
    else
    {
        auto* vec{continueLabels->get(node->label.text)};
        if (vec == nullptr)
            REPORT_ERROR(INVALID_LOOP_LABEL, node->label);
        else
            vec->push_back(code.addJump(OP_JUMP));
    }
}

DEF(EndStmt)
{
    (void) node;
    this->endJumps->push_back(code.addJump(OP_JUMP));
}

DEF(ExprStmt)
{
    if (node->expr == nullptr) return;

    u8 reg{compileExpr(node->expr)};
    if (inRepl && (node->expr->type != E_ASSIGN_EXPR))
        code.addOp(OP_PRINT_VALID, reg);
    freeReg();
}

DEF(BlockStmt)
{
    pushScope();
    for (const StmtUP& stmt : node->block)
        compileStmt(stmt);
    popScope();
}

template<typename NodeT>
std::pair<bool, Compiler::VarInfo> Compiler::checkMutability(
    const NodeT* node,
    const ExprUP& expr
)
{
    if (expr->type != E_VAR_EXPR) return std::make_pair(true, VarInfo{});

    VarExpr* temp{static_cast<VarExpr*>(expr.get())};
    VarInfo info{resolveVariable(temp->name)};

    if (!info.found)
        reportError(VAR_NOT_DEFINED, temp->name);
    else if (info.access == accessFix)
    {
        if constexpr (std::is_same_v<NodeT, AssignExpr>)
            reportError(ASSIGN_FIXED_VARIABLE, node->oper);
        else
            reportError(MOD_FIXED_VARIABLE, node->oper);
    }

    return std::make_pair(
        (info.found && (info.access == accessVar)),
        info
    );
}

Opcode Compiler::getCompoundAssignOpcode(
    const AST::Expression::AssignExpr* node
)
{
    switch (node->oper.type)
    {
        case TOK_PLUS_EQ:       return OP_ADD;
        case TOK_MINUS_EQ:      return OP_SUB;
        case TOK_STAR_EQ:       return OP_MULT;
        case TOK_SLASH_EQ:      return OP_DIV;
        case TOK_PERCENT_EQ:    return OP_MOD;
        case TOK_STAR_STAR_EQ:  return OP_POWER;

        case TOK_AMP_EQ:        return OP_AND;
        case TOK_BAR_EQ:        return OP_OR;
        case TOK_UARROW_EQ:     return OP_XOR;
        case TOK_TILDE_EQ:      return OP_COMP;
        case TOK_LSHIFT_EQ:     return OP_SHIFT_L;
        case TOK_RSHIFT_EQ:     return OP_SHIFT_R;
        default: CH_UNREACHABLE();
    }
}

void Compiler::assignToVar(
    const AssignExpr* node,
    const ExprUP& target,
    u8 valueReg
)
{
    const auto [mut, info] = checkMutability(node, target);
    if (!mut) return;

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssignToVar(node, info, valueReg);
        return;
    }

    emitVariableOp(setVar, info, info.slot, valueReg);
}

void Compiler::compoundAssignToVar(
    const AssignExpr* node,
    const VarInfo& info,
    u8 valueReg
)
{
    u8 varReg{nextReg};
    emitVariableOp(getVar, info, varReg, info.slot);
    reserveReg();

    Opcode op{getCompoundAssignOpcode(node)};
    code.addOp(op, varReg, valueReg);
    emitVariableOp(setVar, info, info.slot, varReg);
    // Since valueReg precedes varReg, we move the latter
    // into the former, keeping the final value in the
    // first-reserved register.
    code.addOp(OP_MOVE_R, valueReg, varReg);
}

void Compiler::assignToElement(
    const AssignExpr* node,
    const ExprUP& target,
    u8 valueReg
)
{
    IndexExpr* item{static_cast<IndexExpr*>(target.get())};
    u8 objReg{compileExpr(item->obj)};
    u8 indexReg{compileExpr(item->index)};

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssignToElement(node, objReg, indexReg, valueReg);
        return;
    }

    code.addOp(OP_SET_INDEX, objReg, indexReg, valueReg);
    nextReg -= 2; // Free the object and index registers.
}

void Compiler::compoundAssignToElement(
    const AssignExpr* node,
    u8 objReg,
    u8 indexReg,
    u8 valueReg
)
{
    u8 elementReg{nextReg};
    code.addOp(OP_GET_INDEX, elementReg, objReg, indexReg);
    reserveReg();

    Opcode op{getCompoundAssignOpcode(node)};
    // E.g., ADD x[0], 1
    code.addOp(op, elementReg, valueReg);
    code.addOp(OP_SET_INDEX, objReg, indexReg, elementReg);
    // Final result should be in the first register reserved,
    // i.e., value register.
    code.addOp(OP_MOVE_R, valueReg, elementReg);
    nextReg -= 3; // Free all registers besides value register.
}

void Compiler::assignToField(
    const AssignExpr* node,
    const ExprUP& target,
    u8 valueReg
)
{
    FieldExpr* expr{static_cast<FieldExpr*>(target.get())};
    u8 objReg{compileExpr(expr->obj)};

    Object field{CH_ALLOC(String, expr->field.text)};
    u8 fieldReg{nextReg};
    code.loadRegConst(field, fieldReg);
    reserveReg();

    if (node->oper.type != TOK_EQUAL)
    {
        compoundAssignToField(node, objReg, fieldReg, valueReg);
        return;
    }

    code.addOp(OP_SET_FIELD, objReg, fieldReg, valueReg);
    nextReg -= 2; // Free the instance and field registers.
}

void Compiler::compoundAssignToField(
    const AssignExpr* node,
    u8 objReg,
    u8 fieldReg,
    u8 valueReg
)
{
    // We use a temporary register to not replace the instance
    // object in objReg.
    u8 tempReg{nextReg};
    code.addOp(OP_GET_FIELD, tempReg, objReg, fieldReg);
    reserveReg();

    Opcode op{getCompoundAssignOpcode(node)};
    code.addOp(op, tempReg, valueReg);
    code.addOp(OP_SET_INDEX, objReg, fieldReg, tempReg);
    // Final result should be in the first register reserved,
    // i.e., value register.
    code.addOp(OP_MOVE_R, valueReg, tempReg);
    nextReg -= 3; // Free all registers besides value register.
}

DEF(MutExpr)
{
    if (node->value->type == E_MUT_EXPR)
    {
        const MutExpr* temp{static_cast<const MutExpr*>(node->value.get())};
        std::string_view nodeType{node->mut ? "mut" : "immut"};
        std::string_view nestedType{temp->mut ? "mut" : "immut"};

        if (node->mut == temp->mut)
        {
            reportPart(
                false, REDUNDANT_MUT_SPECIFIER, node->sourceStart,
                (node->mut ? 3 : 5),
                CH_STR(
                    "'{}' specifier has no effect with another '{}'", nodeType, nestedType
                )
            );
        }
        else
        {
            reportPart(
                false, CONFLICT_MUT_SPECIFIER, node->sourceStart,
                (node->mut ? 3 : 5),
                CH_STR(
                    "'{}' is incompatible with '{}' specifier appearing after it",
                    nodeType, nestedType
                )
            );
        }
    }

    u8 reg{compileExpr(node->value)};
    code.addOp((node->mut ? OP_MUT : OP_IMMUT), reg);
}

DEF(AssignExpr)
{
    if (node->values.size() > 1)
    {
        if (node->targets.size() > node->values.size())
            REPORT_ERROR(UNPACK_TOO_FEW, node->oper);
        else if (!node->unpack && (node->targets.size() < node->values.size()))
            REPORT_ERROR(UNPACK_TOO_MANY, node->oper);
    }

    u8 valueStart{nextReg};
    for (const auto& value : node->values)
        compileExpr(value);
    if ((node->targets.size() > 1) && (node->values.size() == 1))
    {
        code.addOp(OP_UNPACK, valueStart, static_cast<u8>(node->targets.size()));
        emitUnpackState(node->unpack);
    }

    for (u64 i{0}; i < node->targets.size(); i++)
    {
        const auto& target{node->targets[i]};
        switch (target->type)
        {
            case E_VAR_EXPR:    assignToVar(node, target, valueStart + i);      break;
            case E_INDEX_EXPR:  assignToElement(node, target, valueStart + i);  break;
            case E_FIELD_EXPR:  assignToField(node, target, valueStart + i);    break;
            default: CH_UNREACHABLE();
        }
    }

    // For multiple assignment, this will ensure that nextReg
    // is properly restored once ExprStmt calls freeReg().
    // For regular assignment (which may be used as an expression),
    // this places any new values right after the RHS of the assignment,
    // maintaining regular operations.
    nextReg = valueStart + 1;
}

DEF(LogicExpr)
{
    if ((node->oper == TOK_AMP_AMP) || (node->oper == TOK_AND)) // &&, and
    {
        u8 reg{compileExpr(node->left)};
        u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
        nextReg = reg;
        compileExpr(node->right);
        code.patchJump(falseJump);
    }
    else if ((node->oper == TOK_BAR_BAR) || (node->oper == TOK_OR)) // ||, or
    {
        u8 reg{compileExpr(node->left)};
        u64 trueJump{code.addJump(OP_JUMP_TRUE, reg)};
        nextReg = reg;
        compileExpr(node->right);
        code.patchJump(trueJump);
    }
}

DEF(CompareExpr)
{
    u8 firstOper{compileExpr(node->left)};
    u8 secondOper{compileExpr(node->right)};

    Opcode op{};
    switch (node->oper)
    {
        case TOK_IN:
        case TOK_NOT: // not in
            op = OP_IN;
            break;
        case TOK_EQ_EQ:
        case TOK_BANG_EQ:
            op = OP_EQUAL;
            break;
        case TOK_GT:
        case TOK_LT_EQ:
            op = OP_GT;
            break;
        case TOK_LT:
        case TOK_GT_EQ:
            op = OP_LT;
            break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper, secondOper);
    if ((node->oper == TOK_NOT) || (node->oper == TOK_GT_EQ)
        || (node->oper == TOK_LT_EQ) || (node->oper == TOK_BANG_EQ))
    {
        code.addOp(OP_NOT, firstOper);
    }
    freeReg();
}

DEF(BitExpr)
{
    u8 firstOper{compileExpr(node->left)};
    u8 secondOper{compileExpr(node->right)};

    Opcode op{};
    switch (node->oper)
    {
        case TOK_AMP:       op = OP_AND;    break;
        case TOK_BAR:       op = OP_OR;     break;
        case TOK_UARROW:    op = OP_XOR;    break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper, secondOper);
    freeReg();
}

DEF(ShiftExpr)
{
    u8 firstOper{compileExpr(node->left)};
    u8 secondOper{compileExpr(node->right)};

    code.addOp(node->oper == TOK_RIGHT_SHIFT ?
        OP_SHIFT_R : OP_SHIFT_L, firstOper, secondOper);
    freeReg();
}

DEF(BinaryExpr)
{
    u8 firstOper{compileExpr(node->left)};
    u8 secondOper{compileExpr(node->right)};

    Opcode op{};
    switch (node->oper)
    {
        case TOK_PLUS:      op = OP_ADD;    break;
        case TOK_MINUS:     op = OP_SUB;    break;
        case TOK_STAR:      op = OP_MULT;   break;
        case TOK_SLASH:     op = OP_DIV;    break;
        case TOK_PERCENT:   op = OP_MOD;    break;
        case TOK_STAR_STAR: op = OP_POWER;  break;
        case TOK_DOT_DOT:   op = OP_RANGE;  break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper, secondOper);
    freeReg();
}

// We copy the object into two temporary register slots:
// [x][.][.][.][...] -> [...][x][x]
// We then increment/decrement the second register:
// [x][x + 1/x - 1]
// We then store the new value in the object's location:
// [x + 1/x - 1][...][...][x][x + 1/x - 1]
// For post-increment, we move the value in the second
// register into the first one:
// [x + 1/x - 1][x + 1/x - 1]
// For pre-increment, we do nothing (previous value is in
// the correct location).
// In both cases, the result ends up in the first register,
// which is the only reserved register.

void Compiler::_crementVar(
    const UnaryExpr* node
)
{
    const auto [mut, info] = checkMutability(node, node->expr);
    if (!mut)
    {
        reserveReg();
        return;
    }

    u8 tempReg{nextReg};
    emitVariableOp(getVar, info, tempReg, info.slot);
    reserveReg();

    emitVariableOp(getVar, info, nextReg, info.slot);
    code.addOp((node->oper.type == TOK_INCR ?
        OP_INCR : OP_DECR), nextReg);
    emitVariableOp(setVar, info, info.slot, nextReg);

    if (!node->prev)
        code.addOp(OP_MOVE_R, tempReg, nextReg);
}

void Compiler::_crementElement(
    const UnaryExpr* node
)
{
    IndexExpr* item{static_cast<IndexExpr*>(node->expr.get())};
    u8 objReg{compileExpr(item->obj)};
    u8 indexReg{compileExpr(item->index)};

    u8 tempReg{nextReg};
    code.addOp(OP_GET_INDEX, tempReg, objReg, indexReg);
    reserveReg();

    code.addOp(OP_GET_INDEX, nextReg, objReg, indexReg);
    code.addOp((node->oper.type == TOK_INCR ?
        OP_INCR : OP_DECR), nextReg);
    code.addOp(OP_SET_INDEX, objReg, indexReg, nextReg);

    if (!node->prev)
        code.addOp(OP_MOVE_R, tempReg, nextReg);

    // Final result should be in the first register reserved,
    // i.e., object register.
    code.addOp(OP_MOVE_R, objReg, tempReg);
    nextReg -= 2;
}

void Compiler::_crementField(
    const UnaryExpr* node
)
{
    FieldExpr* expr{static_cast<FieldExpr*>(node->expr.get())};
    u8 objReg{compileExpr(expr->obj)};

    Object field{CH_ALLOC(String, expr->field.text)};
    u8 fieldReg{nextReg};
    code.loadRegConst(field, fieldReg);
    reserveReg();

    u8 tempReg{nextReg};
    code.addOp(OP_GET_FIELD, tempReg, objReg, fieldReg);
    reserveReg();

    code.addOp(OP_GET_FIELD, nextReg, objReg, fieldReg);
    code.addOp((node->oper.type == TOK_INCR ?
        OP_INCR : OP_DECR), nextReg);
    code.addOp(OP_SET_FIELD, objReg, fieldReg, nextReg);

    if (!node->prev)
        code.addOp(OP_MOVE_R, tempReg, nextReg);

    // Final result should be in the first register reserved,
    // i.e., instance register.
    code.addOp(OP_MOVE_R, objReg, tempReg);
    nextReg -= 2;
}

DEF(UnaryExpr)
{
    if ((node->oper.type == TOK_INCR) || (node->oper.type == TOK_DECR))
    {
        if (node->expr != nullptr)
        {
            switch (node->expr->type)
            {
                case E_VAR_EXPR:    _crementVar(node);      return;
                case E_INDEX_EXPR:  _crementElement(node);  return;
                case E_FIELD_EXPR:  _crementField(node);    return;
                default: CH_UNREACHABLE();
            }
        }
    }

    u8 firstOper{compileExpr(node->expr)};
    Opcode op{};
    switch (node->oper.type)
    {
        case TOK_MINUS: op = OP_NEG;        break;
        case TOK_BANG:
        case TOK_NOT:   op = OP_NOT;        break;
        case TOK_TILDE: op = OP_COMP;       break;
        default: CH_UNREACHABLE();
    }

    code.addOp(op, firstOper);
    // We don't free a register since unary
    // operators don't use any extra registers.
    // They apply an operator directly onto a
    // register.
}

DEF(IndexExpr)
{
    u64 objectReg{compileExpr(node->obj)};
    u64 indexReg{compileExpr(node->index)};

    // The object should replace the first operand (the second's
    // register is freed up).
    // The two operands are only evaluated to get the element,
    // so they shouldn't stick around afterwards.
    code.addOp(OP_GET_INDEX, objectReg, objectReg, indexReg);
    freeReg();
}

DEF(CallExpr)
{
    if (node->callee == nullptr) return;

    u8 location{};
    if (node->builtin)
    {
        auto* var{static_cast<VarExpr*>(node->callee.get())};
        auto find{Natives::builtins.find(var->name.text)};
        if (find == Natives::builtins.end())
            REPORT_ERROR(BUILTIN_NOT_FOUND, var->name);
        location = static_cast<u8>(find->second);
        reserveReg(); // Reserve a register in place of the function object.
    }
    else
        location = compileExpr(node->callee); // Will reserve a register.

    u8 argsStart{nextReg};
    for (const ExprUP& arg : node->args)
        compileExpr(arg);

    u8 size{static_cast<u8>(node->args.size())};
    code.addOp((node->builtin ? OP_CALL_NAT : OP_CALL_DEF),
        location, argsStart, size);

    // For user-defined functions, the return value replaces the
    // function object.
    // For built-ins, we place the return value in the empty register
    // reserved above.
    nextReg = argsStart;
}

DEF(FieldExpr)
{
    u8 objReg{compileExpr(node->obj)};

    Object field{CH_ALLOC(String, node->field.text)};
    u8 fieldReg{nextReg};
    code.loadRegConst(field, fieldReg);

    code.addOp(OP_GET_FIELD, objReg, objReg, fieldReg);
}

DEF(ScopeExpr)
{
    u8 moduleReg{compileExpr(node->module)};

    Object entry{CH_ALLOC(String, node->entry.text)};
    u8 entryReg{nextReg};
    code.loadRegConst(entry, entryReg);

    code.addOp(OP_GET_ENTRY, moduleReg, moduleReg, entryReg);
}

DEF(IfExpr)
{
    u8 reg{compileExpr(node->condition)};
    u64 falseJump{code.addJump(OP_JUMP_FALSE, reg)};
    freeReg();

    u8 current{compileExpr(node->trueExpr)};
    u64 trueJump{code.addJump(OP_JUMP)};
    code.patchJump(falseJump);

    nextReg = current;
    compileExpr(node->falseExpr);
    code.patchJump(trueJump);
}

DEF(LambdaExpr)
{
    if (node->params.size() > PARAMETER_MAX)
        REPORT_ERROR(HIT_PARAM_MAX, node->params[PARAMETER_MAX].param);

    Compiler miniCompiler{this};
    funcBodyHelper(miniCompiler, node->params, node->body, nextReg,
        std::string{});
    reserveReg();
}

DEF(ListExpr)
{
    u8 listReg{nextReg};
    code.addOp(OP_LIST, listReg);
    reserveReg();

    u8 count{0};
    u8 startReg{nextReg};
    auto extendList = [this, listReg, &count, startReg] {
        code.addOp(OP_EXT_LIST, listReg, startReg, count);
        nextReg = startReg;
        count = 0;
    };

    for (const ExprUP& entry : node->entries)
    {
        compileExpr(entry);
        if (++count == LIST_ENTRY_GROUP)
            extendList();
    }

    if (count > 0) extendList();
}

DEF(TableExpr)
{
    u8 tableReg{nextReg};
    code.addOp(OP_TABLE, tableReg);
    reserveReg();

    u8 count{0};
    u8 startReg{nextReg};
    auto extendTable = [this, tableReg, &count, startReg] {
        code.addOp(OP_EXT_TABLE, tableReg, startReg, count);
        nextReg = startReg;
        count = 0;
    };

    for (const TableExpr::TablePair& pair : node->pairs)
    {
        compileExpr(pair.key);
        compileExpr(pair.value);
        if (++count == TABLE_ENTRY_GROUP)
            extendTable();
    }

    if (count > 0) extendTable();
}

DEF(InstanceExpr)
{
    u8 objReg{compileExpr(node->typeName)};
    code.addOp(OP_INSTANCE, objReg);

    for (const auto& field : node->fields)
    {
        Object name{CH_ALLOC(String, field.name.text)};
        u8 nameReg{nextReg};
        code.loadRegConst(name, nameReg);
        reserveReg();

        u8 initReg{compileExpr(field.init)};
        code.addOp(OP_INIT_FIELD, objReg, nameReg, initReg);
    }

    code.addOp(OP_FINISH_FIELDS, objReg);
    nextReg = objReg + 1; // Reserve a register for the instance object.
}

template<typename NodeT, typename Lambda>
void Compiler::comprehension(
    const NodeT* node,
    Lambda append
)
{
    pushScope();
    u8 varReg{nextReg};
    bool fix{node->header.fix ? accessFix : accessVar};
    for (const auto& var : node->header.vars)
    {
        defVar(std::string{var.text}, nextReg, fix);
        reserveReg();
    }

    u8 iterReg{compileExpr(node->header.iter)};
    code.addOp(OP_MAKE_ITER, varReg, iterReg);
    u64 failJump{code.addJump(OP_JUMP)}; // If we fail to construct an iterator.

    u64 loopStart{code.getLoopStart()};

    u64 whereJump{0};
    if (node->header.where != nullptr)
    {
        u8 whereReg{compileExpr(node->header.where)};
        whereJump = code.addJump(OP_JUMP_FALSE, whereReg);
        freeReg();
    }

    u8 varCount{static_cast<u8>(node->header.vars.size())};
    if (varCount > 1)
    {
        code.addOp(OP_UNPACK, varReg, varCount);
        emitUnpackState(node->header.unpack);
    }
    if (node->header.fix) code.addOp(OP_FIX, varReg, varCount);
    append();

    if (whereJump != 0)
        code.patchJump(whereJump);

    constexpr int UPDATE_ITER_OP_SIZE{5};
    u16 diff{static_cast<u16>(code.codeSize() - loopStart
        + UPDATE_ITER_OP_SIZE)};
    code.addOp(OP_UPDATE_ITER, varReg, iterReg,
        static_cast<u8>((diff >> CHAR_BIT) & CODE_MAX),
        static_cast<u8>(diff & CODE_MAX)
    );

    code.patchJump(failJump);
    popScope();
}

DEF(ListCompExpr)
{
    u8 listReg{nextReg};
    code.addOp(OP_LIST, listReg);
    reserveReg();

    auto append = [this, node, listReg] {
        u8 resultReg{compileExpr(node->expr)};
        code.addOp(OP_EXT_LIST, listReg, resultReg, u8(1));
    };

    comprehension(node, append);
}

DEF(TableCompExpr)
{
    u8 tableReg{nextReg};
    code.addOp(OP_TABLE, tableReg);
    reserveReg();

    auto append = [this, node, tableReg] {
        u8 keyReg{compileExpr(node->key)};
        compileExpr(node->value);
        code.addOp(OP_EXT_TABLE, tableReg, keyReg, u8(1));
    };

    comprehension(node, append);
}

DEF(ReferenceExpr)
{
    VarInfo info{resolveVariable(node->name)};
    if (!info.found)
    {
        REPORT_ERROR(VAR_NOT_DEFINED, node->name,
            "cannot construct reference to undefined variable");
    }

    code.addOp(OP_MAKE_REF, nextReg, static_cast<u8>(info.type),
        info.slot);
    reserveReg();
}

DEF(VarExpr)
{
    VarInfo info{resolveVariable(node->name)};
    if (!info.found)
        REPORT_ERROR(VAR_NOT_DEFINED, node->name);

    emitVariableOp(getVar, info, nextReg, info.slot);
    reserveReg();
}

DEF(StringPartExpr)
{
    size_t start{};
    size_t offset{};

    switch (node->part.type)
    {
        case TOK_INTER_START:
            start = 1, offset = 1;
            break;
        case TOK_INTER_PART:
            start = 0, offset = 0;
            break;
        case TOK_INTER_END:
            start = 0, offset = 1;
            break;
        default:
            CH_UNREACHABLE();
    }

    Object obj{CH_ALLOC(String, parseStringToken(node->part, start, offset))};
    code.loadRegConst(obj, nextReg);
    // The parts of a format string/string interpolation do not
    // exist on their own (the entire string + format arguments
    // is formed at runtime). Thus, the user cannot directly access
    // or modify them, so we don't mark them as immutable.
    reserveReg();
}

DEF(FormatExpr)
{
    u8 partsBegin{nextReg};
    for (const ExprUP& part : node->parts)
        compileExpr(part);

    code.addOp(OP_FORMAT_STR, partsBegin,
        static_cast<u8>(node->parts.size()));

    // Free all registers except first one (containing the
    // final string).
    nextReg = partsBegin + 1;
}

[[nodiscard]]
static std::string getRawString(const std::string_view& text)
{
    size_t start{sizeof("r\"") - 1};
    size_t sizeOffset{start + sizeof("\"") - 1};

    // Skip leading or trailing newlines.
    if (text[start] == '\n')
    {
        start++;
        sizeOffset++;
    }
    if (text[text.size() - 2] == '\n')
        sizeOffset++;

    return std::string{text.substr(start, text.size() - sizeOffset)};
}

DEF(LiteralExpr)
{
    const Token& tok{node->value};

    if (tok.type == TOK_NUM)
    {
        Object obj{tok.content.i};
        code.loadRegConst(obj, nextReg);
        reserveReg();
    }

    else if (tok.type == TOK_NUM_DEC)
    {
        Object obj{tok.content.d};
        code.loadRegConst(obj, nextReg);
        reserveReg();
    }

    else if (tok.type == TOK_STR_LIT)
    {
        Object obj{CH_ALLOC(String, parseStringToken(tok, 1, 2))};
        code.loadRegConst(obj, nextReg);
        code.addOp(OP_IMMUT, nextReg);
        reserveReg();
    }

    else if (tok.type == TOK_RAW_STR)
    {
        Object obj{CH_ALLOC(String, getRawString(tok.text))};
        code.loadRegConst(obj, nextReg);
        code.addOp(OP_IMMUT, nextReg);
        reserveReg();
    }

    else if ((tok.type == TOK_TRUE) || (tok.type == TOK_FALSE))
    {
        bool value{tok.content.b};
        code.loadReg(nextReg, (value ? OP_TRUE : OP_FALSE));
        reserveReg();
    }

    else if (tok.type == TOK_NULL)
    {
        code.loadReg(nextReg, OP_NULL);
        reserveReg();
    }
}

/* General compilation functions. */

u8 Compiler::compileExpr(const ExprUP& node)
{
    if (node == nullptr) return 0; // Dummy return value.

    u64 lastIndex{metadata.size()};
    metadata.push_back(DebugRange{
        code.codeSize(), 0, node->sourceStart, node->sourceEnd
    });

    u8 reg{nextReg};
    switch (node->type)
    {
        case E_MUT_EXPR:        COMPILE(MutExpr);           break;
        case E_ASSIGN_EXPR:     COMPILE(AssignExpr);        break;
        case E_LOGIC_EXPR:      COMPILE(LogicExpr);         break;
        case E_COMPARE_EXPR:    COMPILE(CompareExpr);       break;
        case E_BIT_EXPR:        COMPILE(BitExpr);           break;
        case E_SHIFT_EXPR:      COMPILE(ShiftExpr);         break;
        case E_BINARY_EXPR:     COMPILE(BinaryExpr);        break;
        case E_UNARY_EXPR:      COMPILE(UnaryExpr);         break;
        case E_INDEX_EXPR:      COMPILE(IndexExpr);         break;
        case E_CALL_EXPR:       COMPILE(CallExpr);          break;
        case E_FIELD_EXPR:      COMPILE(FieldExpr);         break;
        case E_SCOPE_EXPR:      COMPILE(ScopeExpr);         break;
        case E_IF_EXPR:         COMPILE(IfExpr);            break;
        case E_LAMBDA_EXPR:     COMPILE(LambdaExpr);        break;
        case E_LIST_EXPR:       COMPILE(ListExpr);          break;
        case E_TABLE_EXPR:      COMPILE(TableExpr);         break;
        case E_INSTANCE_EXPR:   COMPILE(InstanceExpr);      break;
        case E_LIST_COMP_EXPR:  COMPILE(ListCompExpr);      break;
        case E_TABLE_COMP_EXPR: COMPILE(TableCompExpr);     break;
        case E_REF_EXPR:        COMPILE(ReferenceExpr);     break;
        case E_VAR_EXPR:        COMPILE(VarExpr);           break;
        case E_STR_PART_EXPR:   COMPILE(StringPartExpr);    break;
        case E_FORMAT_EXPR:     COMPILE(FormatExpr);        break;
        case E_LITERAL_EXPR:    COMPILE(LiteralExpr);       break;
    }

    metadata[lastIndex].byteEnd = code.codeSize();
    return reg;
}

void Compiler::compileStmt(const StmtUP& node)
{
    if (node == nullptr) return;

    u64 lastIndex{metadata.size()};
    metadata.push_back(DebugRange{
        code.codeSize(), 0, node->sourceStart, node->sourceEnd
    });

    switch (node->type)
    {
        case S_VAR_DECL:    COMPILE(VarDecl);       break;
        case S_FUNC_DECL:   COMPILE(FuncDecl);      break;
        case S_TYPE_DECL:   COMPILE(TypeDecl);      break;
        case S_USE_STMT:    COMPILE(UseStmt);       break;
        case S_IF_STMT:     COMPILE(IfStmt);        break;
        case S_WHILE_STMT:  COMPILE(WhileStmt);     break;
        case S_FOR_STMT:    COMPILE(ForStmt);       break;
        case S_MATCH_STMT:  COMPILE(MatchStmt);     break;
        case S_REPEAT_STMT: COMPILE(RepeatStmt);    break;
        case S_RETURN_STMT: COMPILE(ReturnStmt);    break;
        case S_BREAK_STMT:  COMPILE(BreakStmt);     break;
        case S_CONT_STMT:   COMPILE(ContinueStmt);  break;
        case S_END_STMT:    COMPILE(EndStmt);       break;
        case S_EXPR_STMT:   COMPILE(ExprStmt);      break;
        case S_BLOCK_STMT:  COMPILE(BlockStmt);     break;
    }

    metadata[lastIndex].byteEnd = code.codeSize();
}

ByteCode& Compiler::getCode()
{
    code.setDebugData(id, metadata);
    return code;
}

Function* Compiler::compile(FileID id, const StmtVec& program)
{
    this->id = id;
    code.clear();
    metadata.clear();
    clearDeclarations();
    // Inherit hitError from parser.

    for (const StmtUP& node : program)
        compileStmt(node);

    // Bytecode chunk is only empty upon error.
    if (hitError)
    {
        code.clear();
        // Only clear variables if any were declared.
        // Will clear on the next run.
        if (declaredVars.size() != 0) clearDeclaredVars = true;
    }
    else
        code.addOp(OP_HALT);

    return CH_ALLOC(Function, getCode());
}

const Compiler::VarTable& Compiler::getSymbolTable() const
{
    return *(varLocations.get());
}

#undef DEF
#undef COMPILE
#undef REPORT_ERROR