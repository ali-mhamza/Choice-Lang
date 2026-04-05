#include "../include/parser.h"
#include "../include/astnodes.h"
#include "../include/common.h"
#include "../include/config.h"
#include "../include/error.h"
#include "../include/token.h"
#include <memory>
#include <string>
#include <string_view>
#include <utility> // For std::move.
#include <vector>

using namespace AST::Statement;
using namespace AST::Expression;

#undef REPORT_SYNTAX
#define REPORT_SYNTAX(...)                                          \
    do {                                                            \
        hitError = true;                                            \
        if (syntaxError || (errorCount > COMPILE_ERROR_MAX))        \
            return nullptr;                                         \
        if (errorCount == COMPILE_ERROR_MAX)                        \
            CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");       \
        else                                                        \
            CompileError{__VA_ARGS__}.report();                     \
        syntaxError = true;                                         \
        errorCount++;                                               \
        return nullptr;                                             \
    } while (false)

#undef REPORT_SEMANTIC
#define REPORT_SEMANTIC(...)                                        \
    do {                                                            \
        hitError = true;                                            \
        if (semanticError || (errorCount > COMPILE_ERROR_MAX))      \
            return nullptr;                                         \
        if (errorCount == COMPILE_ERROR_MAX)                        \
            CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");       \
        else                                                        \
            CompileError{__VA_ARGS__}.report();                     \
        semanticError = true;                                       \
        errorCount++;                                               \
        return nullptr;                                             \
    } while (false)

#define MATCH_TOK(...)                              \
    if (!matchError(__VA_ARGS__)) return nullptr;

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
        hitError = true;
        if (!syntaxError && (errorCount < COMPILE_ERROR_MAX))
        {
            CompileError{currentTok, std::string{message}}.report();
            semanticError = true;
        }
        else if (errorCount == COMPILE_ERROR_MAX)
            CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");
        errorCount++;
        return false;
    }

    return true;
}

bool Parser::consumeType()
{
    for (int i{TOK_INT}; i <= TOK_ANY; i++)
    {
        TokenType type{static_cast<TokenType>(i)};
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
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

void Parser::matchType(std::string_view message /* = "" */)
{
    // Must report manually since function is void.
    if (!consumeType())
    {
        hitError = true;
        if (!syntaxError && (errorCount < COMPILE_ERROR_MAX))
        {
            CompileError{currentTok, std::string{message}}.report();
            syntaxError = true;
        }
        else if (errorCount == COMPILE_ERROR_MAX)
            CH_PRINT("COMPILATION ERROR MAXIMUM REACHED.\n");
        errorCount++;
    }
}

StmtUP Parser::declaration()
{
    StmtUP ret{nullptr};
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

    return ret;
}

StmtUP Parser::varDecl()
{
    TokenType declType{previousTok.type};
    consumeTok(TOK_DEF); // In case it's there.

    MATCH_TOK(TOK_IDENTIFIER, "Expect variable name.");
    Token name{previousTok};

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    ExprUP init{nullptr};
    if (consumeTok(TOK_EQUAL))
        init = expression();
    else if (declType == TOK_FIX)
    {
        if (currentTok.type == TOK_SEMICOLON)
        {
            REPORT_SEMANTIC(currentTok,
                "Initializer required for fixed-value variable.");
        }
        else
        {
            REPORT_SYNTAX(currentTok,
                "Expect '=' before initializer for fixed-value variable.");
        }
    }

    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after variable declaration.");
    return std::make_unique<VarDecl>(declType, name, init);
}

StmtUP Parser::funcBodyHelper(bool lambda, vT& params, bool skipParams)
{
    if (!lambda)
        MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after function name.");

    if (!skipParams)
    {
        if (!checkTok(lambda ? TOK_BAR : TOK_RIGHT_PAREN))
        {
            do {
                MATCH_TOK(TOK_IDENTIFIER, "Expect parameter name.");
                params.emplace_back(previousTok);
            } while (consumeTok(TOK_COMMA));
        }

        if (lambda)
        {
            MATCH_TOK(TOK_BAR, "Expect '|' after lambda parameters.");
        }
        else
        {
            MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' to close function signature.");
        }
    }

    MATCH_TOK(TOK_LEFT_BRACE, lambda ?
        "Expect '{' before lambda body." : "Expect '{' before function body.");

    bool prevInFunc{inFunc};
    inFunc = true;
    StmtUP body{blockStmt()};
    inFunc = prevInFunc;

    return body;
}

StmtUP Parser::funDecl()
{
    MATCH_TOK(TOK_IDENTIFIER, "Expect function name.");
    Token name{previousTok};

    vT params{};
    StmtUP body{funcBodyHelper(false, params)};

    return std::make_unique<FuncDecl>(name, params, body);
}

StmtUP Parser::statement()
{   
    if (consumeTok(TOK_IF))
        return ifStmt();
    else if (consumeTok(TOK_WHILE))
        return whileStmt();
    else if (consumeTok(TOK_FOR))
        return forStmt();
    else if (consumeTok(TOK_MATCH))
        return matchStmt();
    else if (consumeTok(TOK_REPEAT))
        return repeatStmt();
    else if (consumeTok(TOK_RETURN))
        return returnStmt();
    else if (consumeTok(TOK_BREAK))
        return breakStmt();
    else if (consumeTok(TOK_LEFT_BRACE))
        return blockStmt();
    else if (consumeTok(TOK_CONT))
        return continueStmt();
    // Consider splitting into their own methods.
    else if (consumeTok(TOK_FALL))
    {
        if (!inMatch)
        {
            REPORT_SEMANTIC(previousTok, "Invalid instruction 'fallthrough'" \
                " outside of match-is structure.");
        }
        MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'fallthrough'.");
        if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
        {
            REPORT_SEMANTIC(currentTok,
                "Cannot have a statement following a 'fallthrough' instruction.");
        }
        fall = true;
        return nullptr;
    }
    else if (consumeTok(TOK_END))
    {
        if (!inMatch)
        {
            REPORT_SEMANTIC(previousTok,
                "Invalid instruction 'end' outside of match-is structure.");
        }
        MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'end'.");
        return std::make_unique<EndStmt>();
    }
    return exprStmt();
}

StmtUP Parser::ifStmt()
{
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'if'.");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

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
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'while'.");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");
    Token label{}; // Default: TOK_EOF.
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "Expect loop label after ':'.");
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
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'for'.");
    MATCH_TOK(TOK_IDENTIFIER, "Expect loop variable identifier.");
    Token var{previousTok};
    MATCH_TOK(TOK_IN, "Expect 'in' keyword after loop identifier.");
    ExprUP iter{expression()};

    ExprUP where{nullptr};
    if (consumeTok(TOK_WHERE))
        where = expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    Token label{}; // Default: TOK_EOF.
    if (consumeTok(TOK_COLON))
    {
        MATCH_TOK(TOK_IDENTIFIER, "Expect loop label after ':'.");
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
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' before match value.");
    ExprUP match{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after match value.");
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before match cases.");

    std::vector<MatchStmt::MatchCase> cases{};
    cases.reserve(MATCH_CASES_MAX);

    bool prevInMatch{inMatch};
    inMatch = true;
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
    {
        if (static_cast<int>(cases.size()) == MATCH_CASES_MAX)
        {
            REPORT_SEMANTIC(currentTok,
                "Too many cases in match-is structure.");
        }

        MATCH_TOK(TOK_IS, "Expect 'is' before case value.");
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
            value = primary();
            // This is a bit too strict.
            // Prohibits variables and even lists.
            // Maybe just keep it as a general expression since the bytecode
            // doesn't change.
            if ((value == nullptr) || (value->type != E_LITERAL_EXPR))
                REPORT_SEMANTIC(errorToken, "Case value must be a literal.");
        }

        MATCH_TOK(TOK_COLON, "Expect ':' before case body.");
        StmtUP body{};
        if (checkTok(TOK_IS) || checkTok(TOK_RIGHT_BRACE))
            body = nullptr;
        else
            body = statement();

        if (defaultCase && consumeTok(TOK_IS))
        {
            REPORT_SEMANTIC(previousTok,
                "Cannot have another case after the default case.");
        }

        // 'fall' updated in statement().
        cases.emplace_back(value, body, fall);
        fall = false; // Reset.
        if (defaultCase)
            break;
    }

    MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after match-is structure.");

    inMatch = prevInMatch;
    return std::make_unique<MatchStmt>(match, cases);
}

StmtUP Parser::repeatStmt()
{
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before 'repeat' block.");
    if (previousTok.type != TOK_LEFT_BRACE) return nullptr;

    StmtUP body{blockStmt()}; // Will consume the '}'.
    MATCH_TOK(TOK_UNTIL, "Expect 'until' condition after 'repeat'.");
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' before 'until' condition.");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after 'until' condition.");
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after repeat-until block.");

    return std::make_unique<RepeatStmt>(condition, body);
}

StmtUP Parser::returnStmt()
{
    if (!inFunc)
        REPORT_SEMANTIC(previousTok, "Cannot use 'return' outside a function.");

    Token keyword{previousTok};
    ExprUP expr{nullptr};
    if (!checkTok(TOK_SEMICOLON))
        expr = tuple();
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after return statement.");
    return std::make_unique<ReturnStmt>(keyword, expr);
}

StmtUP Parser::breakStmt()
{
    // Add error handling.

    Token name{};
    if (consumeTok(TOK_IDENTIFIER))
        name = previousTok;
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'break'.");
    return std::make_unique<BreakStmt>(name);
}

StmtUP Parser::continueStmt()
{
    // Add error handling.

    Token name{};
    if (consumeTok(TOK_IDENTIFIER))
        name = previousTok;
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after 'continue'.");
    return std::make_unique<ContinueStmt>(name);
}

StmtUP Parser::blockStmt()
{
    StmtVec block{};
    block.reserve(10);
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
        block.push_back(declaration());
    MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after block.");
    return std::make_unique<BlockStmt>(block);
}

StmtUP Parser::exprStmt()
{
    StmtUP ptr{std::make_unique<ExprStmt>(expression())};
    MATCH_TOK(TOK_SEMICOLON, "Expect ';' after expression.");
    return ptr;
}

ExprUP Parser::tuple()
{
    ExprVec entries{};
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

    return std::make_unique<TupleExpr>(entries);
}

ExprUP Parser::expression()
{
    return assignment();
}

ExprUP Parser::assignment()
{
    ExprUP target{logicOr()};
    if (IS_ASSIGN_TOK(currentTok.type))
    {
        nextTok();
        Token oper{previousTok};
        if ((target == nullptr) || (target->type != E_VAR_EXPR)) // Temporary.
            REPORT_SEMANTIC(previousTok, "Invalid assignment target.");
        target = std::make_unique<AssignExpr>(target, oper, expression());
    }

    return target;
}

ExprUP Parser::logicOr()
{
    ExprUP expr{logicAnd()};
    while (consumeToks(TOK_BAR_BAR, TOK_OR))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<LogicExpr>(expr, oper, logicAnd());
    }

    return expr;
}

ExprUP Parser::logicAnd()
{
    ExprUP expr{equality()};
    while (consumeToks(TOK_AMP_AMP, TOK_AND))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<LogicExpr>(expr, oper, equality());
    }

    return expr;
}

ExprUP Parser::equality()
{
    ExprUP expr{comparison()};
    while (consumeToks(TOK_EQ_EQ, TOK_BANG_EQ))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<CompareExpr>(expr, oper, comparison());
    }

    return expr;
}

ExprUP Parser::comparison()
{
    ExprUP expr{bitOr()};
    while (consumeToks(TOK_GT, TOK_GT_EQ, TOK_LT, TOK_LT_EQ, TOK_IN)
            || (consumeTok(TOK_NOT) && checkTok(TOK_IN)))
    {
        TokenType oper{previousTok.type};
        if (oper == TOK_NOT) nextTok();
        expr = std::make_unique<CompareExpr>(expr, oper, bitOr());
    }

    return expr;
}

ExprUP Parser::bitOr()
{
    ExprUP expr{bitXor()};
    while (consumeTok(TOK_BAR))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BitExpr>(expr, oper, bitXor());
    }

    return expr;
}

ExprUP Parser::bitXor()
{
    ExprUP expr{bitAnd()};
    while (consumeTok(TOK_UARROW))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BitExpr>(expr, oper, bitAnd());
    }

    return expr;
}

ExprUP Parser::bitAnd()
{
    ExprUP expr{shift()};
    while (consumeTok(TOK_AMP))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BitExpr>(expr, oper, shift());
    }

    return expr;
}

ExprUP Parser::shift()
{
    ExprUP expr{sum()};
    while (consumeToks(TOK_RIGHT_SHIFT, TOK_LEFT_SHIFT))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<ShiftExpr>(expr, oper, sum());
    }

    return expr;
}

ExprUP Parser::sum()
{
    ExprUP expr{product()};
    while (consumeToks(TOK_PLUS, TOK_MINUS))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, product());
    }

    return expr;
}

ExprUP Parser::product()
{
    ExprUP expr{unary()};
    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, unary());
    }

    return expr;
}

ExprUP Parser::unary()
{    
    if (consumeToks(TOK_INCR, TOK_DECR, TOK_MINUS,
        TOK_BANG, TOK_NOT, TOK_TILDE))
    {
        Token oper{previousTok};
        return std::make_unique<UnaryExpr>(oper, unary(), false);
    }

    return exponent();
}

ExprUP Parser::exponent()
{
    ExprUP expr{call()};
    while (consumeTok(TOK_STAR_STAR))
    {
        TokenType oper{previousTok.type};
        expr = std::make_unique<BinaryExpr>(expr, oper, exponent());
    }

    return expr;
}

ExprUP Parser::call()
{
    // Callee does not need to be an identifier.
    // Just has to evaluate to a callable object.
    // Exception: builtin with ! token.

    ExprUP expr{post()};
    if ((currentTok.type == TOK_BANG) && (expr != nullptr)
        && (expr->type != E_VAR_EXPR))
    {
        REPORT_SEMANTIC(currentTok, "Built-in functions must be called by name.");
    }

    if (consumeToks(TOK_BANG, TOK_LEFT_PAREN))
    {
        bool builtin{false};
        if (previousTok.type == TOK_BANG)
        {
            MATCH_TOK(TOK_LEFT_PAREN, "Invalid placement for token '!'.");
            builtin = true;
        }

        ExprVec args{};
        while (!checkTok(TOK_RIGHT_PAREN) && !checkTok(TOK_EOF))
        {
            do {
                args.push_back(expression());
            } while (consumeTok(TOK_COMMA));
        }
        MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' following function arguments.");
        return std::make_unique<CallExpr>(expr, args, builtin, previousTok);
    }
    else
        return expr;
}

ExprUP Parser::post()
{
    ExprUP expr{primary()};

    if (consumeToks(TOK_INCR, TOK_DECR))
    {
        if ((expr == nullptr) || (expr->type != E_VAR_EXPR)) // Temporary.
            REPORT_SEMANTIC(previousTok, "Invalid increment/decrement target.");
        do {
            Token oper{previousTok};
            expr = std::make_unique<UnaryExpr>(oper, std::move(expr), true);
        } while (consumeToks(TOK_INCR, TOK_DECR));
    }

    return expr;
}

ExprUP Parser::ifExpr()
{
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' before condition.");
    ExprUP condition{expression()};
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    // Possibly return if no brace found?
    MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
    ExprUP trueBranch{expression()};
    MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");

    ExprUP falseBranch{nullptr}; // To avoid warnings.
    if (consumeTok(TOK_ELIF))
        falseBranch = ifExpr();
    else if (consumeTok(TOK_ELSE))
    {
        MATCH_TOK(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
        falseBranch = expression();
        MATCH_TOK(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");
    }
    else
    {
        REPORT_SEMANTIC(currentTok,
            "A conditional expression must have a false-case branch.");
    }

    return std::make_unique<IfExpr>(condition, trueBranch, falseBranch);
}

ExprUP Parser::lambda(bool skipParams)
{
    vT params{};
    StmtUP body{funcBodyHelper(true, params, skipParams)};
    return std::make_unique<LambdaExpr>(params, body);
}

ExprUP Parser::comprehension()
{
    MATCH_TOK(TOK_LEFT_PAREN, "Expect '(' after 'for'.");
    MATCH_TOK(TOK_IDENTIFIER, "Expect loop variable identifier.");
    Token var{previousTok};
    MATCH_TOK(TOK_IN, "Expect 'in' keyword after loop identifier.");
    ExprUP iter{expression()};

    ExprUP where{nullptr};
    if (consumeTok(TOK_WHERE))
        where = expression();
    MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    MATCH_TOK(TOK_COLON, "Expect ':' before comprehension expression.");
    ExprUP expr{expression()};
    MATCH_TOK(TOK_RIGHT_BRACKET, "Expect ']' to conclude list comprehension.");

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
    MATCH_TOK(TOK_RIGHT_BRACKET, "Expect ']' to conclude list literal.");

    return std::make_unique<ListExpr>(entries);
}

ExprUP Parser::primary()
{
    nextTok();
    TokenType type{previousTok.type};

    if (IS_LITERAL_TOK(type))
        return std::make_unique<LiteralExpr>(previousTok);
    
    else if (type == TOK_IDENTIFIER)
        return std::make_unique<VarExpr>(previousTok);
    
    else if (type == TOK_LEFT_PAREN)
    {
        ExprUP expr{expression()};
        MATCH_TOK(TOK_RIGHT_PAREN, "Expect ')' after grouped expression.");
        return expr;
    }

    else if (type == TOK_IF)
        return ifExpr();

    else if (type == TOK_BAR || type == TOK_BAR_BAR)
        return lambda(type == TOK_BAR_BAR);

    else if (type == TOK_LEFT_BRACKET)
        return list();
    
    REPORT_SYNTAX(previousTok, "Invalid token in current position.");
    return nullptr;
}

StmtVec& Parser::parseToAST(const vT& tokens)
{
    program.clear(); // In case we want to reuse the parser.
    if (tokens.empty()) return program;

    it = tokens.begin();
    currentTok = tokens[0];
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