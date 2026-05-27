#include "../include/parser.h"
#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/config.h"
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

#define CAN_ASSIGN(node) \
    (((node)->type == E_VAR_EXPR) || ((node)->type == E_INDEX_EXPR))

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
    for (u8 type{TOK_INT_T}; type <= TOK_CLASS_T; type++)
    {
        if (consumeTok(static_cast<TokenType>(type)))
            return true;
    }

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

void Parser::reset()
{
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
            case TOK_FUNC:      case TOK_RETURN:
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
    diagEngine.source = ErrorSource::PARSER;
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
    diagEngine.source = ErrorSource::PARSER;
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

StmtUP Parser::declaration()
{
    StmtUP ret{nullptr};
    u64 start{currentTok.byteOffset};

    if (consumeTok(TOK_SEMICOLON)) // Empty statement.
        return ret;
    else if (consumeToks(TOK_MAKE, TOK_FIX))
        ret = varDecl();
    else if (consumeTok(TOK_FUNC))
        ret = funDecl();
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
    consumeTok(TOK_DEF); // In case it's there.

    MATCH_TOK(TOK_IDENTIFIER, "expect variable name");
    Token name{previousTok};
    CONSUME_VAR_TYPE();

    ExprUP init{nullptr};
    if (consumeTok(TOK_EQUAL))
        init = expression();
    else if (declType == TOK_FIX)
    {
        if (currentTok.type == TOK_SEMICOLON)
            REPORT_SEMANTIC(MISSING_INITIALIZER, currentTok);
        else
        {
            REPORT_SYNTAX(WRONG_TOKEN_FOUND, currentTok,
                "expect '=' before initializer for fixed-value variable");
        }
    }

    MATCH_TOK(TOK_SEMICOLON, "expect ';' after variable declaration");
    return std::make_unique<VarDecl>(declType, name, init);
}

StmtUP Parser::funcBodyHelper(vT& params)
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after function name");

    if (!checkTok(TOK_RIGHT_PAREN))
    {
        do {
            if (consumeTok(TOK_FIX)) params.emplace_back(previousTok);
            MATCH_TOK(TOK_IDENTIFIER, "expect parameter name");
            params.emplace_back(previousTok);
            CONSUME_VAR_TYPE();
        } while (consumeTok(TOK_COMMA));
    }

    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' to close function signature");
    CONSUME_RETURN_TYPE();

    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before function body");

    bool prevInFunc{inFunc};
    inFunc = true;
    StmtUP body{blockStmt()};
    inFunc = prevInFunc;

    return body;
}

StmtUP Parser::funDecl()
{
    MATCH_TOK(TOK_IDENTIFIER, "expect function name");
    Token name{previousTok};

    vT params{};
    StmtUP body{funcBodyHelper(params)};

    return std::make_unique<FuncDecl>(name, params, body);
}

StmtUP Parser::statement()
{
    StmtUP stmt{nullptr};
    u64 start{currentTok.byteOffset};

    if (consumeTok(TOK_IF))
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
        fall = true;
        return nullptr;
    }
    else if (consumeTok(TOK_END))
    {
        if (!inMatch)
            REPORT_SEMANTIC(INVALID_END, previousTok);
        MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'end'");
        stmt = std::make_unique<EndStmt>();
    }
    else
        stmt = exprStmt();

    setStmtLocation(stmt, start);
    return stmt;
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
    Token label{}; // Default: TOK_EOF.
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "expect loop label after ':'");
        label = previousTok;
    }
    StmtUP body{statement()};
    StmtUP elseClause{nullptr};
    if (consumeTok(TOK_ELSE))
        elseClause = statement();

    return std::make_unique<WhileStmt>(condition, label, body, elseClause);
}

StmtUP Parser::forStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after 'for'");
    MATCH_TOK(TOK_IDENTIFIER, "expect loop variable identifier");

    Token var{previousTok};
    CONSUME_VAR_TYPE();

    MATCH_TOK(TOK_IN, "expect 'in' keyword after loop variable");
    ExprUP iter{expression()};

    ExprUP where{nullptr};
    if (consumeTok(TOK_WHERE))
        where = expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after condition");

    Token label{}; // Default: TOK_EOF.
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "expect loop label after ':'");
        label = previousTok;
    }

    StmtUP body{statement()};
    StmtUP elseClause{nullptr};
    if (consumeTok(TOK_ELSE))
        elseClause = statement();

    return std::make_unique<ForStmt>(var, iter, where, label, body, elseClause);
}

StmtUP Parser::matchStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' before match value");
    ExprUP match{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after match value");
    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before match cases");

    std::vector<MatchStmt::MatchCase> cases{};
    cases.reserve(MATCH_CASES_MAX);

    bool prevInMatch{inMatch};
    inMatch = true;
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
    {
        if (static_cast<int>(cases.size()) == MATCH_CASES_MAX)
            REPORT_SEMANTIC(HIT_MATCH_CASE_MAX, currentTok);

        MATCH_TOK(TOK_IS, "expect 'is' before case value");
        ExprUP value{};
        bool defaultCase{false};

        if (consumeTok(TOK_QMARK))
        {
            value = nullptr;
            defaultCase = true;
        }
        else
        {
            Token errorToken{currentTok};
            value = expression();
        }

        MATCH_TOK(TOK_COLON, "expect ':' before case body");
        StmtUP body{};
        if (checkTok(TOK_IS) || checkTok(TOK_RIGHT_BRACE))
            body = nullptr;
        else
            body = statement();

        if (defaultCase && consumeTok(TOK_IS))
            REPORT_SEMANTIC(CASE_AFTER_DEFAULT, previousTok);

        // 'fall' updated in statement().
        cases.emplace_back(value, body, fall);
        fall = false; // Reset.
        if (defaultCase)
            break;
    }

    MATCH_TOK(TOK_RIGHT_BRACE, "expect '}' after match-is structure");

    inMatch = prevInMatch;
    return std::make_unique<MatchStmt>(match, cases);
}

StmtUP Parser::repeatStmt()
{
    Token label{};
    if (consumeTok(TOK_IDENTIFIER))
        label = previousTok;

    MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before 'repeat' block");

    StmtUP body{blockStmt()}; // Will consume the '}'.
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

    Token keyword{previousTok};
    ExprUP expr{nullptr};
    if (!checkTok(TOK_SEMICOLON))
        expr = returnExpr();
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after return statement");
    return std::make_unique<ReturnStmt>(keyword, expr);
}

StmtUP Parser::breakStmt()
{
    // Add error handling.

    Token name{};
    if (consumeTok(TOK_IDENTIFIER))
        name = previousTok;
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'break'");
    return std::make_unique<BreakStmt>(name);
}

StmtUP Parser::continueStmt()
{
    // Add error handling.

    Token name{};
    if (consumeTok(TOK_IDENTIFIER))
        name = previousTok;
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after 'continue'");
    return std::make_unique<ContinueStmt>(name);
}

StmtUP Parser::blockStmt()
{
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
    ExprUP expr{expression()};
    setExprLocation(expr, start);
    MATCH_TOK(TOK_SEMICOLON, "expect ';' after expression");
    return std::make_unique<ExprStmt>(expr);
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
    ExprUP target{logicOr()};
    if (IS_ASSIGN_TOK(currentTok.type))
    {
        nextTok();
        Token oper{previousTok};
        if ((target == nullptr) || !CAN_ASSIGN(target))
            REPORT_SEMANTIC(INVALID_ASSIGN_TARGET, previousTok);
        target = std::make_unique<AssignExpr>(target, oper, logicOr());
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
    while (consumeTok(TOK_BAR))
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
    if (consumeToks(TOK_INCR, TOK_DECR, TOK_MINUS,
        TOK_BANG, TOK_NOT, TOK_TILDE))
    {
        Token oper{previousTok};
        expr = std::make_unique<UnaryExpr>(oper, unary(), false);
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
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, exponent());
    }

    setExprLocation(expr, start);
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
        if ((expr == nullptr) || (expr->type != E_VAR_EXPR))
            REPORT_SEMANTIC(BUILTIN_CALL_NO_NAME, previousTok);
        MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after function name and '!'");
        builtin = true;
    }

    ExprVec args{};
    while (!checkTok(TOK_RIGHT_PAREN) && !checkTok(TOK_EOF))
    {
        do {
            if (args.size() == CODE_MAX)
                REPORT_SEMANTIC(HIT_ARGS_MAX, currentTok);

            if (consumeTok(TOK_STAR))
            {
                u64 offset{previousTok.byteOffset};
                MATCH_TOK(TOK_IDENTIFIER, "expect reference name");
                args.push_back(
                    std::make_unique<ReferenceExpr>(offset, previousTok)
                );
                setExprLocation(args.back(), offset);
                continue;
            }

            args.push_back(expression());
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
            expr = call(std::move(expr), start);
        else if (consumeTok(TOK_LEFT_BRACKET))
        {
            ExprUP index{expression()};
            MATCH_TOK(TOK_RIGHT_BRACKET, "expect ']' following index");
            expr = std::make_unique<IndexExpr>(expr, index);
        }
        else if (consumeToks(TOK_INCR, TOK_DECR))
        {
            if ((expr == nullptr) || !CAN_ASSIGN(expr))
                REPORT_SEMANTIC(INVALID_INCR_DECR_TARGET, previousTok);
            Token oper{previousTok};
            expr = std::make_unique<UnaryExpr>(oper, std::move(expr), true);
        }
        else
            return expr;

        setExprLocation(expr, start);
        start = currentTok.byteOffset;
    }
}

ExprUP Parser::ifExpr()
{
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

StmtUP Parser::lambdaBodyHelper(vT& params, bool skipParams)
{
    if (!skipParams)
    {
        if (!checkTok(TOK_BAR))
        {
            do {
                if (consumeTok(TOK_FIX)) params.emplace_back(previousTok);
                MATCH_TOK(TOK_IDENTIFIER, "expect parameter name");
                params.emplace_back(previousTok);
                CONSUME_VAR_TYPE();
            } while (consumeTok(TOK_COMMA));
        }

        MATCH_TOK(TOK_BAR, "expect '|' after lambda parameters");
    }

    StmtUP body{};
    u64 start{currentTok.byteOffset};
    if (consumeTok(TOK_THICK_ARROW))
    {
        ExprUP result{expression()};
        body = std::make_unique<ReturnStmt>(Token{}, result);
        setStmtLocation(body, start);
    }
    else
    {
        CONSUME_RETURN_TYPE();
        MATCH_TOK(TOK_LEFT_BRACE, "expect '{' before lambda body");

        bool prevInFunc{inFunc};
        inFunc = true;
        body = blockStmt();
        inFunc = prevInFunc;
    }

    return body;
}

ExprUP Parser::lambda(bool skipParams)
{
    vT params{};
    StmtUP body{lambdaBodyHelper(params, skipParams)};
    return std::make_unique<LambdaExpr>(params, body);
}

ExprUP Parser::comprehension()
{
    MATCH_TOK(TOK_LEFT_PAREN, "expect '(' after 'for'");
    MATCH_TOK(TOK_IDENTIFIER, "expect loop variable identifier");

    Token var{previousTok};
    CONSUME_VAR_TYPE();

    MATCH_TOK(TOK_IN, "expect 'in' keyword after loop variable");
    ExprUP iter{expression()};

    ExprUP where{nullptr};
    if (consumeTok(TOK_WHERE))
        where = expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "expect ')' after condition");

    MATCH_TOK(TOK_COLON, "expect ':' before comprehension expression");
    ExprUP expr{expression()};
    MATCH_TOK(TOK_RIGHT_BRACKET, "expect ']' to conclude list comprehension");

    return std::make_unique<ComprehensionExpr>(var, iter, where, expr);
}

ExprUP Parser::list()
{
    ExprVec entries{};
    entries.reserve(LIST_ENTRY_GROUP); // Minimal size to start off with.
    if (!checkTok(TOK_RIGHT_BRACKET))
    {
        if (consumeTok(TOK_FOR))
            return comprehension();

        do {
            entries.emplace_back(expression());
        } while (consumeTok(TOK_COMMA));
    }
    MATCH_TOK(TOK_RIGHT_BRACKET, "expect ']' to conclude list literal");

    return std::make_unique<ListExpr>(entries);
}

ExprUP Parser::table()
{
    std::vector<TableExpr::TablePair> pairs{};
    pairs.reserve(TABLE_ENTRY_GROUP);

    if (!checkTok(TOK_RIGHT_BRACE))
    {
        do {
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

ExprUP Parser::formatString()
{
    ExprVec parts{};
    parts.emplace_back(std::make_unique<StringPartExpr>(previousTok));
    setExprLocation(parts.back(), previousTok.byteOffset);
    while (!checkTok(TOK_INTER_END) && !checkTok(TOK_EOF))
    {
        if (consumeTok(TOK_INTER_PART))
        {
            parts.emplace_back(std::make_unique<StringPartExpr>(previousTok));
            setExprLocation(parts.back(), previousTok.byteOffset);
        }
        else
            parts.emplace_back(expression());
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
        expr = std::make_unique<VarExpr>(previousTok);

    else if (IS_INTER_TOK(type))
        expr = formatString();

    else if (type == TOK_LEFT_PAREN)
    {
        expr = expression();
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
    errorCount = 0;

    while (!checkTok(TOK_EOF))
        program.push_back(declaration());

    // Do not clear the AST nodes, even if errors occurred.
    // This allows the AST compiler to report errors on valid
    // nodes that parsed just fine.
    return program;
}