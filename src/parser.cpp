/*
 * All code for the parser in the interpreter pipeline.
 * The parser takes an array of tokens from the lexer and
 * returns an AST to be compiled, while performing necessary
 * error-reporting.
 */

#include "../include/parser.h"
#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/diagnostic.h"
#include "../include/token.h"
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

using namespace AST::Statement;
using namespace AST::Expression;

#define REPORT_SYNTAX(...)          \
    do {                            \
        reportSyntax(__VA_ARGS__);  \
        return nullptr;             \
    } while (false)

#define REPORT_SEMANTIC(...)            \
    do {                                \
        reportSemantic(__VA_ARGS__);    \
        return nullptr;                 \
    } while (false)

#define MATCH_TOK(...)                              \
    if (!matchError(__VA_ARGS__)) return nullptr;

#define CONSUME_VAR_TYPE()                      \
    if (consumeTok(TOK_COLON)) consumeType();

#define CONSUME_RETURN_TYPE()                   \
    if (consumeTok(TOK_RARROW)) consumeType();

#define CAN_ASSIGN(node)                                                            \
    (((node)->type == ExprType::VarExpr) || ((node)->type == ExprType::IndexExpr)   \
        || ((node)->type == ExprType::FieldExpr))

#define FIELD_OPER_TOK TOK_DOT

/* DepthCounter type. */

#define CHECK_DEPTH(tok)            \
    DepthCounter counter_{id, tok}; \
    if (counter_.hitError) {        \
        hitError = true;            \
        syntaxError = true;         \
        return nullptr;             \
    }

u64 Parser::DepthCounter::exprDepthCount{0};
u64 Parser::DepthCounter::blockDepthCount{0};

Parser::DepthCounter::DepthCounter(FileID id, const Token& token)
{
    if (token.type == TOK_LEFT_BRACE) // Checking a block.
        checkBlockDepth(id, token);
    else
        checkExprDepth(id, token);
}

Parser::DepthCounter::~DepthCounter()
{
    if (incrementedExpr) exprDepthCount--;
    if (incrementedBlock) blockDepthCount--;
}

void Parser::DepthCounter::checkExprDepth(FileID id, const Token& token)
{
    if (exprDepthCount > MAX_EXPR_NEST_DEPTH)
        hitError = true;
    else if (++exprDepthCount > MAX_EXPR_NEST_DEPTH)
    {
        diagEngine.source = ErrorSource::Parser;
        hitError = true;

        diagEngine.recordError(id, HIT_EXPR_NESTING_MAX,
            token, CH_STR("maximum depth is {}", MAX_EXPR_NEST_DEPTH)
        );
        incrementedExpr = false;
    }
    else
        incrementedExpr = true;
}

void Parser::DepthCounter::checkBlockDepth(FileID id, const Token& token)
{
    if (blockDepthCount > MAX_EXPR_NEST_DEPTH)
        hitError = true;
    else if (++blockDepthCount > MAX_EXPR_NEST_DEPTH)
    {
        diagEngine.source = ErrorSource::Parser;
        hitError = true;

        diagEngine.recordError(id, HIT_BLOCK_NESTING_MAX,
            token, CH_STR("maximum depth is {}", MAX_BLOCK_SCOPE_DEPTH)
        );
        incrementedBlock = false;
    }
    else
        incrementedBlock = true;
}

void Parser::DepthCounter::reset()
{
    exprDepthCount = 0;
    blockDepthCount = 0;
}

/* Parsing helpers. */

void Parser::nextTok()
{
    if (currentTok.type != TOK_EOF)
    {
        previousTok = currentTok;
        currentTok = *(++it);
    }
}

bool Parser::checkTok(TokenType type) const
{
    return (currentTok.type == type);
}

bool Parser::consumeTok(TokenType type)
{
    if (checkTok(type))
    {
        nextTok();
        return true;
    }

    return false;
}

template<typename... Type>
bool Parser::consumeToks(Type... toks)
{
    for (TokenType type : {toks...})
    {
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

bool Parser::matchError(TokenType type, std::string_view message)
{
    if (!consumeTok(type))
    {
        DiagCode code{
            currentTok.type == TOK_EOF ? UNEXPECTED_INPUT_END : WRONG_TOKEN_FOUND
        };
        reportSyntax(code, currentTok, message);
        return false;
    }

    return true;
}

bool Parser::consumeTypename()
{
    if (consumeTok(TOK_IDENTIFIER)) return true;
    reportSyntax(WRONG_TOKEN_FOUND, currentTok, "expect variable type");
    return false;
}

void Parser::consumeType()
{
    // For reference types, e.g., *Int.
    consumeTok(TOK_STAR);

    // For possible types, e.g., <Int | String>.
    if (consumeTok(TOK_LT))
    {
        do {
            consumeType();
        } while (consumeTok(TOK_BAR));
        (void) matchError(TOK_GT, "expect closing '>' after types");
    }

    // For collections of types, e.g., (Int, String).
    else if (consumeTok(TOK_LEFT_PAREN))
    {
        do {
            consumeType();
        } while (consumeTok(TOK_COMMA));
        (void) matchError(TOK_RIGHT_PAREN, "expect closing ')' after type group");
    }

    // For basic typenames or sequence types, e.g., Int
    // or List[Int].
    else
    {
        consumeTypename();
        if (consumeTok(TOK_LEFT_BRACKET))
        {
            consumeType();
            (void) matchError(TOK_RIGHT_BRACKET, "expect closing ']'");
        }
        else if (checkTok(TOK_LEFT_PAREN))
            consumeType();
    }

    // For function types with specified return types, e.g.,
    // Func(Int, Boolean) -> String.
    if (consumeTok(TOK_RARROW))
        consumeType();

    // For nullable types, e.g., Int?.
    consumeTok(TOK_QMARK);
}

void Parser::skipOrphanedConditionalBranch()
{
    bool syntax{syntaxError}, semantic{semanticError};
    // To silence any errors in the blocks.
    syntaxError = semanticError = true;
    if (previousTok.type == TOK_ELIF)
    {
        consumeTok(TOK_LEFT_PAREN);
        (void) expression();
        consumeTok(TOK_RIGHT_PAREN);
    }

    consumeTok(TOK_LEFT_BRACE);
    (void) statement();

    syntaxError = syntax;
    semanticError = semantic;
}

void Parser::reset()
{
    nextTok();
    while (!checkTok(TOK_EOF))
    {
        if ((previousTok.type == TOK_SEMICOLON)
            || (previousTok.type == TOK_RIGHT_BRACE))
        {
            return;
        }

        switch (currentTok.type)
        {
            case TOK_IF:        case TOK_ELIF:      case TOK_ELSE:
            case TOK_WHILE:     case TOK_FOR:       case TOK_REPEAT:
            case TOK_UNTIL:     case TOK_BREAK:     case TOK_CONT:
            case TOK_MATCH:     case TOK_IS:        case TOK_FALL:
            case TOK_END:       case TOK_MAKE:      case TOK_FIX:
            case TOK_FUNC:      case TOK_TYPE:      case TOK_USE:
            case TOK_RETURN:    case TOK_AT:
            case TOK_IDENTIFIER:
            case TOK_LEFT_BRACE:
                return;
            default:
                nextTok();
        }
    }
}

void Parser::reportSyntax(
    DiagCode code,
    const Token& token,
    std::string_view message
)
{
    diagEngine.source = ErrorSource::Parser;
    hitError = true;
    if (syntaxError) return;
    syntaxError = true;

    diagEngine.recordError(id, code, token, std::string{message});
}

void Parser::reportSemantic(
    DiagCode code,
    const Token& token,
    std::string_view message
)
{
    diagEngine.source = ErrorSource::Parser;
    hitError = true;
    if (semanticError) return;
    semanticError = true;
    diagEngine.recordError(id, code, token, std::string{message});
}

void Parser::setStmtLocation(StmtUP& stmt, u64 start)
{
    if (stmt != nullptr)
    {
        // We only store these offsets if the statement
        // has not already been assigned any.

        if (stmt->sourceStart == UINT64_MAX)
            stmt->sourceStart = start;
        if (stmt->sourceEnd == UINT64_MAX)
        {
            stmt->sourceEnd = previousTok.byteOffset
                + previousTok.text.size();
        }
    }
}

void Parser::setExprLocation(ExprUP& expr, u64 start)
{
    if (expr != nullptr)
    {
        // We only store these offsets if the expression
        // has not already been assigned any.

        if (expr->sourceStart == UINT64_MAX)
            expr->sourceStart = start;
        if (expr->sourceEnd == UINT64_MAX)
        {
            expr->sourceEnd = previousTok.byteOffset
                + previousTok.text.size();
        }
    }
}

VarAttr Parser::consumeAttributes()
{
    VarAttr attr{};

    while (consumeTok(TOK_AT))
    {
        if (!matchError(TOK_LEFT_PAREN, "expect '(' before attribute(s)"))
            break;

        do {
            nextTok();
            switch (previousTok.type)
            {
                case TOK_PRIVATE:   markAttribute(attr, DeclAttr::Private);     break;
                case TOK_STATIC:    markAttribute(attr, DeclAttr::Static);      break;
                case TOK_COMPUTED:  markAttribute(attr, DeclAttr::Computed);    break;
                case TOK_CLOSED:    markAttribute(attr, DeclAttr::Closed);      break;
                case TOK_TEST:      markAttribute(attr, DeclAttr::Test);        break;
                default:
                    reportSemantic(INVALID_ATTR, previousTok);
                    break;
            }
        } while (consumeTok(TOK_COMMA));

        if (!matchError(TOK_RIGHT_PAREN, "expect ')' after attribute(s)"))
            break;
    }

    return attr;
}

void Parser::parseVariableList(
    vT& vars,
    AST::UnpackState& unpack,
    std::string_view errorMsg
)
{
    bool done{false};

    do {
        if (consumeTok(TOK_ELLIPSIS))
        {
            unpack.unpackIgnore = true;
            break;
        }

        if (!matchError(TOK_IDENTIFIER, errorMsg)) return;
        vars.push_back(previousTok);

        if (consumeTok(TOK_ELLIPSIS))
        {
            unpack.unpackLastVar = true;
            done = true;
        }

        CONSUME_VAR_TYPE();
    } while (!done && consumeTok(TOK_COMMA));
}

/* Main parsing functions. */

StmtUP Parser::declaration()
{
    VarAttr attr{consumeAttributes()};
    StmtUP ret{nullptr};
    u64 start{currentTok.byteOffset};

    if (consumeTok(TOK_SEMICOLON)) // Empty statement.
        return ret;
    else if (consumeToks(TOK_MAKE, TOK_FIX))
    {
        ret = varDecl();
        static_cast<VarDecl*>(ret.get())->attr = attr;
    }
    else if (consumeTok(TOK_FUNC))
    {
        ret = funcDecl();
        static_cast<FuncDecl*>(ret.get())->attr = attr;
    }
    else if (consumeTok(TOK_TYPE))
    {
        ret = typeDecl();
        static_cast<TypeDecl*>(ret.get())->attr = attr;
    }
    else
        ret = statement();

    if (syntaxError || semanticError)
    {
        reset();
        syntaxError = semanticError = false;
    }

    setStmtLocation(ret, start);
    return ret;
}

StmtUP Parser::varDecl()
{
    TokenType declType{previousTok.type};
    vT names{};
    AST::UnpackState unpack{};
    parseVariableList(names, unpack, "expect variable name");

    Token oper{};
    ExprVec values{};
    if (consumeTok(TOK_EQUAL))
    {
        oper = previousTok;
        do {
            values.push_back(expression());
        } while (consumeTok(TOK_COMMA));
    }
    else if (declType == TOK_FIX)
    {
        if (currentTok.type == TOK_SEMICOLON)
            REPORT_SEMANTIC(MISSING_INITIALIZER, currentTok);
        else
        {
            REPORT_SYNTAX(WRONG_TOKEN_FOUND, currentTok,
                "expect '=' before initializer for fixed-value variable(s)");
        }
    }

    MATCH_TOK(TOK_SEMICOLON, "expect ';' after variable declaration(s)");
    return std::make_unique<VarDecl>((declType == TOK_FIX), names, unpack,
        oper, values);
}

bool Parser::parseParams(std::vector<AST::Param>& params)
{
    bool startedDefaultArgs{false}, variadic{false};
    if (!checkTok(TOK_RIGHT_PAREN))
    {
        do {
            if (variadic)
            {
                reportSyntax(PARAM_AFTER_VARIADIC, previousTok);
                return false;
            }

            bool fix{consumeTok(TOK_FIX)};
            if (!matchError(TOK_IDENTIFIER, "expect parameter name")) return false;
            Token param{previousTok};

            ExprUP defaultVal{};
            if (consumeTok(TOK_EQUAL))
            {
                defaultVal = expression();
                startedDefaultArgs = true;
            }
            else if (consumeTok(TOK_ELLIPSIS))
                variadic = true;
            else if (startedDefaultArgs)
            {
                reportSyntax(EXPECT_DEFAULT_PARAM, param);
                return false;
            }

            params.emplace_back(fix, variadic, param, defaultVal);
            CONSUME_VAR_TYPE();
        } while (consumeTok(TOK_COMMA));
    }

    return true;
}

StmtUP Parser::funcBodyHelper(std::vector<AST::Param>& params)
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after function name");
    if (!parseParams(params)) return nullptr;

    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' to close function signature");
    CONSUME_RETURN_TYPE();

    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before function body");

    bool func{inFunc};
    inFunc = true;
    StmtUP body{blockStmt()};
    inFunc = func;

    return body;
}

StmtUP Parser::funcDecl()
{
    MATCH_TOK(TOK_IDENTIFIER, "expect function name");
    Token name{previousTok};

    std::vector<AST::Param> params{};
    StmtUP body{funcBodyHelper(params)};

    return std::make_unique<FuncDecl>(name, params, body);
}

StmtUP Parser::typeDecl()
{
    MATCH_TOK(TOK_IDENTIFIER, "expect type name");
    Token name{previousTok};

    std::vector<TypeDecl::Field> fields{};
    StmtVec methods{};
    if (!consumeTok(TOK_SEMICOLON))
    {
        MATCH_TOK(TOK_LEFT_BRACE, "expect '{' or ';' after type name");
        if (!checkTok(TOK_FUNC) && !checkTok(TOK_RIGHT_BRACE))
        {
            do {
                VarAttr attr{consumeAttributes()};
                bool fix{consumeTok(TOK_FIX)};
                MATCH_TOK(TOK_IDENTIFIER, "expect field name");
                Token name{previousTok};
                CONSUME_VAR_TYPE();

                ExprUP init{nullptr};
                if (consumeTok(TOK_EQUAL))
                    init = expression();
                else if (fix)
                    REPORT_SEMANTIC(MISSING_INITIALIZER, currentTok);
                fields.emplace_back(attr, fix, name, init);
            } while (consumeTok(TOK_COMMA));
        }

        while (consumeTok(TOK_FUNC))
        {
            bool constructor{inConstructor};
            if (currentTok.text == CH_CONSTRUCTOR)
                inConstructor = true;
            methods.push_back(funcDecl());
            inConstructor = constructor;
        }
        MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' to conclude type declaration");
    }

    return std::make_unique<TypeDecl>(name, fields, methods);
}

StmtUP Parser::statement()
{
    StmtUP stmt{nullptr};
    u64 start{currentTok.byteOffset};

    if (consumeTok(TOK_USE))
        stmt = useStmt();
    else if (consumeTok(TOK_IF))
        stmt = ifStmt();
    else if (consumeTok(TOK_WHILE))
        stmt = whileStmt();
    else if (consumeTok(TOK_FOR))
        stmt = forStmt();
    else if (consumeTok(TOK_MATCH))
        stmt = matchStmt();
    else if (consumeTok(TOK_REPEAT))
        stmt = repeatStmt();
    else if (consumeTok(TOK_RETURN))
        stmt = returnStmt();
    else if (consumeTok(TOK_BREAK))
        stmt = breakStmt();
    else if (consumeTok(TOK_LEFT_BRACE))
        stmt = blockStmt();
    else if (consumeTok(TOK_CONT))
        stmt = continueStmt();
    // Consider splitting into their own methods.
    else if (consumeTok(TOK_FALL))
    {
        if (!inMatch)
            REPORT_SEMANTIC(INVALID_FALLTHROUGH, previousTok);
        MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'fallthrough'");
        if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
            REPORT_SEMANTIC(STMT_AFTER_FALLTHROUGH, currentTok);
        fallthrough = true;
        return nullptr;
    }
    else if (consumeTok(TOK_END))
    {
        if (!inMatch)
            REPORT_SEMANTIC(INVALID_END, previousTok);
        MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'end'");
        stmt = std::make_unique<EndStmt>();
    }
    else if (consumeToks(TOK_ELIF, TOK_ELSE)) // Special error case.
    {
        reportSyntax(INVALID_TOKEN, previousTok, "unexpected conditional branch");
        skipOrphanedConditionalBranch();
        return nullptr;
    }
    else
        stmt = exprStmt();

    setStmtLocation(stmt, start);
    return stmt;
}

UseStmt::Entry Parser::parseModuleEntry()
{
    if (!matchError(TOK_IDENTIFIER, "expect module entry name"))
        return {};

    Token name{previousTok};
    Token alias{};
    if (consumeTok(TOK_AS))
    {
        if (!matchError(TOK_IDENTIFIER, "expect alias for module entry"))
            return {};
        alias = previousTok;
    }

    return { name, alias };
}

StmtUP Parser::useStmt()
{
    MATCH_TOK(TOK_IDENTIFIER, "expect module name");
    Token module{previousTok};
    Token directory{}, alias{};
    std::vector<UseStmt::Entry> entries{};

    if (consumeTok(TOK_SCOPE))
    {
        MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before module entry list");
        do {
            auto entry{parseModuleEntry()};
            // Error occurred.
            if (!entry.name) return nullptr;

            entries.push_back(entry);
        } while (consumeTok(TOK_COMMA));
        MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' after module entry list");
    }

    if (consumeTok(TOK_FROM))
    {
        MATCH_TOK(TOK_STR_LIT, "expect module directory path");
        directory = previousTok;
    }
    if (consumeTok(TOK_AS))
    {
        if (!entries.empty())
            REPORT_SYNTAX(ALIAS_SPEC_MODULE, previousTok);
        MATCH_TOK(TOK_IDENTIFIER, "expect module alias");
        alias = previousTok;
    }

    MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'use' statement");
    return std::make_unique<UseStmt>(module, directory, alias, entries);
}

StmtUP Parser::ifStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after 'if'");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after condition");

    StmtUP trueBranch{statement()};
    StmtUP falseBranch{nullptr};
    if (consumeTok(TOK_ELIF))
        falseBranch = ifStmt();
    else if (consumeTok(TOK_ELSE))
        falseBranch = statement();
    return std::make_unique<IfStmt>(condition, trueBranch, falseBranch);
}

StmtUP Parser::whileStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after 'while'");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after condition");
    Token label{};
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "expect loop label after ':'");
        label = previousTok;
    }

    bool loop{inLoop};
    inLoop = true;
    StmtUP body{statement()};
    inLoop = loop;

    StmtUP elseClause{nullptr};
    if (consumeTok(TOK_ELSE))
        elseClause = statement();

    return std::make_unique<WhileStmt>(condition, label, body, elseClause);
}

AST::LoopHeader Parser::parseLoopHeader()
{
    if (!matchError(TOK_LEFT_PAREN, "expect '(' after 'for'"))
        return {};
    bool fix{consumeTok(TOK_FIX)};

    vT vars{};
    AST::UnpackState unpack{};
    parseVariableList(vars, unpack, "expect loop variable identifier");

    if (!matchError(TOK_IN, "expect 'in' keyword after loop variable"))
        return {};
    ExprUP iter{expression()};

    ExprUP where{nullptr};
    if (consumeTok(TOK_WHERE))
        where = expression();
    if (!matchError(TOK_RIGHT_PAREN, "expect ')' after condition"))
        return {};

    return AST::LoopHeader{fix, vars, unpack, iter, where};
}

StmtUP Parser::forStmt()
{
    AST::LoopHeader header{parseLoopHeader()};
    Token label{};
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "expect loop label after ':'");
        label = previousTok;
    }

    bool loop{inLoop};
    inLoop = true;
    StmtUP body{statement()};
    inLoop = loop;

    StmtUP elseClause{nullptr};
    if (consumeTok(TOK_ELSE))
        elseClause = statement();

    return std::make_unique<ForStmt>(header, label, body, elseClause);
}

StmtUP Parser::matchStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' before match value");
    ExprUP matchValue{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after match value");
    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before match cases");

    std::vector<MatchStmt::MatchCase> cases{};
    cases.reserve(MATCH_CASES_MAX);

    bool match{inMatch};
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
    {
        if (cases.size() == MATCH_CASES_MAX)
            REPORT_SEMANTIC(HIT_MATCH_CASE_MAX, currentTok);

        MATCH_TOK(TOK_IS, "expect 'is' before case value");
        bool defaultCase{consumeTok(TOK_QMARK)};
        ExprUP caseValue{defaultCase ? nullptr : expression()};

        MATCH_TOK(TOK_COLON, "expect ':' before case body");
        StmtUP caseBody{nullptr};
        if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
        {
            inMatch = true; // Before any potential 'fallthrough' or 'end' statements.
            caseBody = statement();
            inMatch = match;
        }

        if (defaultCase && consumeTok(TOK_IS))
            REPORT_SEMANTIC(CASE_AFTER_DEFAULT, previousTok);

        // 'fallthrough' updated in statement().
        cases.emplace_back(caseValue, caseBody, fallthrough);
        fallthrough = false; // Reset.
        if (defaultCase) break;
    }

    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' after match-is structure");
    return std::make_unique<MatchStmt>(matchValue, cases);
}

StmtUP Parser::repeatStmt()
{
    Token label{};
    if (consumeTok(TOK_IDENTIFIER))
        label = previousTok;

    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before 'repeat' block");

    bool loop{inLoop};
    inLoop = true;
    StmtUP body{blockStmt()}; // Will consume the '}'.
    inLoop = loop;

    MATCH_TOK(TOK_UNTIL, "expect 'until' condition after 'repeat'");
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' before 'until' condition");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after 'until' condition");
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after repeat-until block");

    return std::make_unique<RepeatStmt>(condition, label, body);
}

StmtUP Parser::returnStmt()
{
    if (!inFunc)
        REPORT_SEMANTIC(INVALID_RETURN, previousTok);
    else if (inConstructor)
        REPORT_SEMANTIC(RETURN_IN_CTOR, previousTok);

    Token keyword{previousTok};
    ExprUP expr{nullptr};
    if (!checkTok(TOK_SEMICOLON))
        expr = returnExpr();
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after return statement");
    return std::make_unique<ReturnStmt>(keyword, expr);
}

StmtUP Parser::breakStmt()
{
    if (!inLoop)
    {
        REPORT_SEMANTIC(inComprehension ? BREAK_IN_COMPREHEN : INVALID_BREAK,
            previousTok);
    }

    Token name{};
    if (consumeTok(TOK_IDENTIFIER))
        name = previousTok;
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'break'");
    return std::make_unique<BreakStmt>(name);
}

StmtUP Parser::continueStmt()
{
    if (!inLoop)
    {
        REPORT_SEMANTIC(inComprehension ? CONT_IN_COMPREHEN : INVALID_CONTINUE,
            previousTok);
    }

    Token name{};
    if (consumeTok(TOK_IDENTIFIER))
        name = previousTok;
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'continue'");
    return std::make_unique<ContinueStmt>(name);
}

StmtUP Parser::blockStmt()
{
    CHECK_DEPTH(previousTok);

    u64 start{previousTok.byteOffset}; // The left '{'.
    StmtVec block{};
    block.reserve(10);

    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
        block.push_back(declaration());
    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' after block");

    StmtUP blockStmt{std::make_unique<BlockStmt>(block)};
    setStmtLocation(blockStmt, start);

    return blockStmt;
}

StmtUP Parser::exprStmt()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{multiAssignment()};
    setExprLocation(expr, start);
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after expression");
    return std::make_unique<ExprStmt>(expr);
}

// Multiple assignment and return expressions both live
// "above" the expression grammar. This means that, while
// they can contain other expressions, no other expressions
// can contain them. Expression nesting automatically skips
// them during parsing for this reason, going to 'expression'
// instead.
// This allows us to restrict the usage of the comma-separated
// syntax that they rely on, which would otherwise interfere
// with other syntax (e.g., function calls or lists) if allowed
// in every expression context.
// Multiple assignment is restricted to a statement-level construct,
// while return expressions are restricted further to return
// statements only.

ExprUP Parser::multiAssignment()
{
    u64 start{currentTok.byteOffset};
    ExprVec targets{};
    AST::UnpackState unpack{};
    ExprVec values{};

    ExprUP target{expression()};
    if ((target != nullptr) && CAN_ASSIGN(target) && consumeTok(TOK_COMMA))
    {
        targets.push_back(std::move(target));
        do {
            if (consumeTok(TOK_ELLIPSIS))
            {
                unpack.unpackIgnore = true;
                break;
            }

            // Expressions with 'mut' or 'immut' and/or assignments
            // can't be on the LHS, and assignments will consume the
            // '=' that separates the LHS and RHS.
            targets.push_back(logicOr());

            if (consumeTok(TOK_ELLIPSIS))
            {
                unpack.unpackLastVar = true;
                break;
            }
        } while (consumeTok(TOK_COMMA));

        MATCH_TOK(TOK_EQUAL, "expect '=' after multiple assignment targets");
        Token oper{previousTok};
        for (u64 i{0}; i < targets.size(); i++)
        {
            if (!CAN_ASSIGN(targets[i]))
            {
                REPORT_SEMANTIC(INVALID_ASSIGN_TARGET, previousTok,
                    CH_STR("target {} does not support assignment", i + 1));
            }
        }

        do {
            values.push_back(expression());
        } while (consumeTok(TOK_COMMA));

        target = std::make_unique<AssignExpr>(targets, unpack, oper, values);
    }

    setExprLocation(target, start);
    return target;
}

ExprUP Parser::returnExpr()
{
    ExprVec entries{};
    u64 start{currentTok.byteOffset};
    ExprUP entry{expression()};

    if (consumeTok(TOK_COMMA))
    {
        entries.emplace_back(std::move(entry));
        do {
            entries.emplace_back(expression());
        } while (consumeTok(TOK_COMMA));
    }
    else
        return entry;

    ExprUP expr{std::make_unique<ListExpr>(entries)};
    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::expression()
{
    return mutation();
}

ExprUP Parser::mutation()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{nullptr};

    if (consumeToks(TOK_MUT, TOK_IMMUT))
    {
        CHECK_DEPTH(previousTok);
        expr = std::make_unique<MutExpr>(
            (previousTok.type == TOK_MUT),
            mutation()
        );
    }
    else
        expr = assignment();

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::assignment()
{
    u64 start{currentTok.byteOffset};
    ExprVec targets{};
    AST::UnpackState unpack{};
    ExprVec values{};

    ExprUP target{logicOr()};
    if (IS_ASSIGN_TOK(currentTok.type))
    {
        nextTok();
        Token oper{previousTok};
        if ((target == nullptr) || !CAN_ASSIGN(target))
            REPORT_SEMANTIC(INVALID_ASSIGN_TARGET, previousTok);

        CHECK_DEPTH(previousTok);
        targets.push_back(std::move(target));
        values.push_back(expression());
        target = std::make_unique<AssignExpr>(targets, unpack, oper, values);
    }

    setExprLocation(target, start);
    return target;
}

ExprUP Parser::logicOr()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{logicAnd()};
    while (consumeToks(TOK_BAR_BAR, TOK_OR))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<LogicExpr>(expr, oper, logicAnd());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::logicAnd()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{equality()};
    while (consumeToks(TOK_AMP_AMP, TOK_AND))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<LogicExpr>(expr, oper, equality());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::equality()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{comparison()};
    while (consumeToks(TOK_EQ_EQ, TOK_BANG_EQ))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<CompareExpr>(expr, oper, comparison());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::comparison()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{range()};
    while (consumeToks(TOK_GT, TOK_GT_EQ, TOK_LT, TOK_LT_EQ, TOK_IN)
            || (consumeTok(TOK_NOT) && checkTok(TOK_IN)))
    {
        TokenType oper{previousTok.type};
        if (oper == TOK_NOT) nextTok();
        expr = std::make_unique<CompareExpr>(expr, oper, range());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::range()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{bitOr()};
    if (consumeTok(TOK_DOT_DOT))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, bitOr());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::bitOr()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{bitXor()};
    while (!inLambdaParams && consumeTok(TOK_BAR))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BitExpr>(expr, oper, bitXor());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::bitXor()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{bitAnd()};
    while (consumeTok(TOK_UARROW))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BitExpr>(expr, oper, bitAnd());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::bitAnd()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{shift()};
    while (consumeTok(TOK_AMP))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BitExpr>(expr, oper, shift());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::shift()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{sum()};
    while (consumeToks(TOK_RIGHT_SHIFT, TOK_LEFT_SHIFT))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<ShiftExpr>(expr, oper, sum());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::sum()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{product()};
    while (consumeToks(TOK_PLUS, TOK_MINUS))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, product());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::product()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{unary()};
    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, unary());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::unary()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{nullptr};

    if (consumeToks(TOK_INCR, TOK_DECR))
    {
        CHECK_DEPTH(previousTok);
        Token oper{previousTok};
        ExprUP target{unary()};

        if ((target == nullptr) || !CAN_ASSIGN(target))
            REPORT_SEMANTIC(INVALID_INCR_DECR_TARGET, oper);
        expr = std::make_unique<UnaryExpr>(oper, std::move(target), false);
    }
    else if (consumeToks(TOK_MINUS, TOK_BANG, TOK_NOT, TOK_TILDE))
    {
        CHECK_DEPTH(previousTok);
        Token oper{previousTok};
        expr = std::make_unique<UnaryExpr>(oper, unary());
    }
    else
        expr = exponent();

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::exponent()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{post()};
    while (consumeTok(TOK_STAR_STAR))
    {
        CHECK_DEPTH(previousTok);
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, exponent());
    }

    setExprLocation(expr, start);
    return expr;
}

ExprUP Parser::reference()
{
    const Token oper{previousTok};
    CHECK_DEPTH(previousTok);
    ExprUP expr{expression()};

    if ((expr == nullptr) || !CAN_ASSIGN(expr))
        REPORT_SYNTAX(REF_NOT_ASSIGN, oper);

    expr = std::make_unique<RefExpr>(expr);
    setExprLocation(expr, oper.byteOffset);
    return expr;
}

ExprUP Parser::call(ExprUP&& expr, u64 start)
{
    // Callee does not need to be an identifier.
    // Just has to evaluate to a callable object.
    // Exception: builtin with ! token.

    bool builtin{false};
    if (previousTok.type == TOK_BANG)
    {
        if ((expr == nullptr) || (expr->type != ExprType::VarExpr))
            REPORT_SEMANTIC(BUILTIN_CALL_NO_NAME, previousTok);
        MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after function name and '!'");
        builtin = true;
    }

    ExprVec args{};
    if (!checkTok(TOK_RIGHT_PAREN) && !checkTok(TOK_EOF))
    {
        do {
            if (args.size() == CODE_MAX)
                REPORT_SEMANTIC(HIT_ARGS_MAX, currentTok);

            args.push_back(consumeTok(TOK_STAR) ? reference() : expression());
        } while (consumeTok(TOK_COMMA));
    }

    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' following function arguments");
    expr = std::make_unique<CallExpr>(expr, args, builtin);
    setExprLocation(expr, start);

    return std::move(expr);
}

ExprUP Parser::post()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{primary()};

    while (true)
    {
        if (consumeToks(TOK_BANG, TOK_LEFT_PAREN))
        {
            CHECK_DEPTH(previousTok);
            expr = call(std::move(expr), start);
        }
        else if (consumeTok(TOK_LEFT_BRACKET))
        {
            CHECK_DEPTH(previousTok);
            ExprUP index{expression()};
            MATCH_TOK(TOK_RIGHT_BRACKET, "expect ']' following index");
            expr = std::make_unique<IndexExpr>(expr, index);
        }
        else if (consumeToks(TOK_INCR, TOK_DECR))
        {
            CHECK_DEPTH(previousTok);
            if ((expr == nullptr) || !CAN_ASSIGN(expr))
                REPORT_SEMANTIC(INVALID_INCR_DECR_TARGET, previousTok);
            Token oper{previousTok};
            expr = std::make_unique<UnaryExpr>(oper, std::move(expr), true);
        }
        else if (consumeTok(FIELD_OPER_TOK))
        {
            CHECK_DEPTH(previousTok);
            MATCH_TOK(TOK_IDENTIFIER, "expect field name");
            expr = std::make_unique<FieldExpr>(expr, previousTok);
        }
        else if (consumeTok(TOK_SCOPE))
        {
            CHECK_DEPTH(previousTok);
            MATCH_TOK(TOK_IDENTIFIER, "expect name of module entry");
            expr = std::make_unique<ScopeExpr>(expr, previousTok);
        }
        else
            return expr;

        setExprLocation(expr, start);
        start = currentTok.byteOffset;
    }
}

ExprUP Parser::ifExpr()
{
    CHECK_DEPTH(previousTok);

    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' before condition");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after condition");

    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before conditional expression");
    ExprUP trueBranch{expression()};
    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' after conditional expression");

    ExprUP falseBranch{nullptr}; // To avoid warnings.
    if (consumeTok(TOK_ELIF))
        falseBranch = ifExpr();
    else if (consumeTok(TOK_ELSE))
    {
        MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before conditional expression");
        falseBranch = expression();
        MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' after conditional expression");
    }
    else
        REPORT_SEMANTIC(IF_EXPR_MISSING_FALSE, currentTok);

    return std::make_unique<IfExpr>(condition, trueBranch, falseBranch);
}

StmtUP Parser::lambdaBodyHelper(
    std::vector<AST::Param>& params,
    bool skipParams
)
{
    if (!skipParams)
    {
        bool lambdaState{inLambdaParams};
        inLambdaParams = true;
        bool success{parseParams(params)};
        inLambdaParams = lambdaState; // Reset before potentially returning.
        if (!success) return nullptr;

        MATCH_TOK(TOK_BAR, "expect '|' after lambda parameters");
    }

    StmtUP body{};
    u64 start{currentTok.byteOffset};
    if (consumeTok(TOK_THICK_ARROW))
    {
        CHECK_DEPTH(currentTok);
        ExprUP result{expression()};
        body = std::make_unique<ReturnStmt>(Token{}, result);
        setStmtLocation(body, start);
    }
    else
    {
        CONSUME_RETURN_TYPE();
        MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before lambda body");

        bool func{inFunc};
        inFunc = true;
        body = blockStmt();
        inFunc = func;
    }

    return body;
}

ExprUP Parser::lambda(bool skipParams)
{
    CHECK_DEPTH(previousTok);

    std::vector<AST::Param> params{};
    StmtUP body{lambdaBodyHelper(params, skipParams)};
    return std::make_unique<LambdaExpr>(params, body);
}

ExprUP Parser::list()
{
    CHECK_DEPTH(previousTok);

    ExprVec entries{};
    entries.reserve(LIST_ENTRY_GROUP); // Minimal size to start off with.
    if (!checkTok(TOK_RIGHT_BRACKET))
    {
        if (consumeTok(TOK_FOR))
            return listComprehension();

        do {
            CHECK_DEPTH(currentTok);
            entries.emplace_back(expression());
        } while (consumeTok(TOK_COMMA));
    }
    MATCH_TOK(TOK_RIGHT_BRACKET, "expect ']' to conclude list literal");

    return std::make_unique<ListExpr>(entries);
}

ExprUP Parser::table()
{
    CHECK_DEPTH(previousTok);

    std::vector<TableExpr::TablePair> pairs{};
    pairs.reserve(TABLE_ENTRY_GROUP);

    if (!checkTok(TOK_RIGHT_BRACE))
    {
        if (consumeTok(TOK_FOR))
            return tableComprehension();

        do {
            CHECK_DEPTH(currentTok);

            MATCH_TOK(TOK_LEFT_PAREN, "expect '(' before table pair");
            ExprUP key{expression()};
            MATCH_TOK(TOK_COMMA, "expect ',' after key");
            ExprUP value{expression()};
            MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after table pair");

            pairs.emplace_back(TableExpr::TablePair{
                std::move(key), std::move(value)
            });
        } while (consumeTok(TOK_COMMA));
    }
    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' to conclude table literal");

    return std::make_unique<TableExpr>(pairs);
}

ExprUP Parser::instance()
{
    ExprUP typeName{std::make_unique<VarExpr>(previousTok)};
    nextTok(); // Skip the checked open brace '{'.

    std::vector<InstanceExpr::Field> fields{};
    if (!checkTok(TOK_RIGHT_BRACE))
    {
        do {
            MATCH_TOK(TOK_IDENTIFIER, "expect field name");
            Token name{previousTok};
            MATCH_TOK(TOK_EQUAL, "expect field initializer");
            CHECK_DEPTH(previousTok);
            fields.emplace_back(name, expression());
        } while (consumeTok(TOK_COMMA));
    }

    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' to conclude instance");
    return std::make_unique<InstanceExpr>(typeName, fields);
}

ExprUP Parser::listComprehension()
{
    // No depth-checking here, since list() takes care of that.

    AST::LoopHeader header{parseLoopHeader()};
    MATCH_TOK(TOK_COLON, "expect ':' before comprehension expression");

    bool comprehension{inComprehension};
    inComprehension = true;
    ExprUP expr{expression()};
    inComprehension = comprehension;

    MATCH_TOK(TOK_RIGHT_BRACKET, "expect ']' to conclude list comprehension");
    return std::make_unique<ListCompExpr>(header, expr);
}

ExprUP Parser::tableComprehension()
{
    AST::LoopHeader header{parseLoopHeader()};
    MATCH_TOK(TOK_COLON, "expect ':' before comprehension pair");

    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' before key");
    bool comprehension{inComprehension};

    inComprehension = true;
    ExprUP key{expression()};
    inComprehension = comprehension;
    MATCH_TOK(TOK_COMMA, "expect ',' after key");

    inComprehension = true;
    ExprUP value{expression()};
    inComprehension = comprehension;
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after value");

    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' to conclude table comprehension");
    return std::make_unique<TableCompExpr>(header, key, value);
}

ExprUP Parser::formatString()
{
    CHECK_DEPTH(previousTok);

    ExprVec parts{};
    parts.emplace_back(std::make_unique<StringPartExpr>(previousTok));
    setExprLocation(parts.back(), previousTok.byteOffset);
    while (!checkTok(TOK_INTER_END) && !checkTok(TOK_EOF))
    {
        if (hitError) return nullptr; // To not get into an infinite loop.

        if (consumeTok(TOK_INTER_PART))
        {
            parts.emplace_back(std::make_unique<StringPartExpr>(previousTok));
            setExprLocation(parts.back(), previousTok.byteOffset);
        }
        else
        {
            CHECK_DEPTH(currentTok);
            parts.emplace_back(expression());
        }
    }

    MATCH_TOK(TOK_INTER_END, "expect end of format string");
    parts.emplace_back(std::make_unique<StringPartExpr>(previousTok));
    setExprLocation(parts.back(), previousTok.byteOffset);
    return std::make_unique<FormatExpr>(parts);
}

ExprUP Parser::primary()
{
    u64 start{currentTok.byteOffset};
    ExprUP expr{nullptr};

    nextTok();
    TokenType type{previousTok.type};

    if (IS_LITERAL_TOK(type))
        expr = std::make_unique<LiteralExpr>(previousTok);

    else if (type == TOK_IDENTIFIER)
    {
        if (checkTok(TOK_LEFT_BRACE))
            expr = instance();
        else
            expr = std::make_unique<VarExpr>(previousTok);
    }

    else if (IS_INTER_TOK(type))
        expr = formatString();

    else if (type == TOK_LEFT_PAREN)
    {
        CHECK_DEPTH(previousTok);

        bool lambdaState{inLambdaParams};
        inLambdaParams = false;
        expr = expression();
        inLambdaParams = lambdaState;

        MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after grouped expression");
    }

    else if (type == TOK_IF)
        expr = ifExpr();

    else if (type == TOK_BAR || type == TOK_BAR_BAR)
        expr = lambda(type == TOK_BAR_BAR);

    else if (type == TOK_LEFT_BRACKET)
        expr = list();

    else if (type == TOK_LEFT_BRACE)
        expr = table();

    else
        REPORT_SYNTAX(INVALID_TOKEN, previousTok);

    setExprLocation(expr, start);
    return expr;
}

StmtVec& Parser::parseToAST(FileID id, const vT& tokens)
{
    program.clear(); // In case we want to reuse the parser.
    // Abort upon lexer error or empty input.
    if (tokens.empty() || tokens.front().type == TOK_EOF)
        return program;

    this->id = id;
    it = tokens.begin();
    currentTok = tokens[0];

    hitError = false;
    syntaxError = false;
    semanticError = false;

    while (!checkTok(TOK_EOF))
    {
        DepthCounter::reset();
        program.push_back(declaration());
    }

    // Do not clear the AST nodes, even if errors occurred.
    // This allows the AST compiler to report errors on valid
    // nodes that parsed just fine.
    return program;
}

#undef REPORT_SYNTAX
#undef REPORT_SEMATIC
#undef MATCH_TOK
#undef CONSUME_VAR_TYPE
#undef CONSUME_RETURN_TYPE
#undef CAN_ASSIGN
#undef FIELD_OPER_TOK
#undef CHECK_DEPTH