#ifdef COMP_AST

#include "../include/parser.h"
#include "../include/astnodes.h"
#include "../include/error.h"
#include "../include/token.h"
#include <memory>
using namespace AST::Statement;
using namespace AST::Expression;

Parser::Parser() :
    inMatch(false), fall(false) {}

void Parser::nextTok()
{
    if (currentTok.type != TOK_EOF)
    {
        previousTok = currentTok;
        currentTok = *(++it);
    }
}

bool Parser::checkTok(TokenType type)
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
    for (TokenType type : {toks ...})
    {
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

// Basic implementation.
void Parser::matchError(TokenType type, std::string_view message)
{
    if (!consumeTok(type))
        throw CompileError(currentTok, std::string(message));
}

bool Parser::consumeType()
{
    for (int i = TOK_INT; i <= TOK_ANY; i++)
    {
        TokenType type = static_cast<TokenType>(i);
        if (checkTok(type))
            return consumeTok(type);
    }

    return false;
}

TokenType Parser::matchType(std::string_view message /* = "" */)
{
    if (!consumeType())
        throw CompileError(currentTok, std::string(message));
    return previousTok.type;
}

StmtUP Parser::declaration()
{
    if (consumeToks(TOK_MAKE, TOK_FIX))
        return varDecl();
    else
        return statement();
}

StmtUP Parser::varDecl()
{
    TokenType declType = previousTok.type;
    consumeTok(TOK_DEF); // In case it's there.

    matchError(TOK_IDENTIFIER, "Expect variable name.");
    Token name = previousTok;

    if (consumeTok(TOK_COLON))
        matchType("Expect variable type.");

    ExprUP init = nullptr;
    if (consumeTok(TOK_EQUAL))
        init = expression();
    else if (declType == TOK_FIX)
        throw CompileError(currentTok, 
            "Initializer required for fixed-value variable.");

    matchError(TOK_SEMICOLON, "Expect ';' after variable declaration.");

    return StmtUP(std::make_unique<VarDecl>(declType, name, 
        std::move(init)));
}

StmtUP Parser::statement()
{   
    if (consumeTok(TOK_IF))
        return ifStmt();
    else if (consumeTok(TOK_WHILE))
        return whileStmt();
    else if (consumeTok(TOK_MATCH))
        return matchStmt();
    else if (consumeTok(TOK_REPEAT))
        return repeatStmt();
    else if (consumeTok(TOK_LEFT_BRACE))
        return blockStmt();
    else if (consumeTok(TOK_FALL))
    {
        if (!inMatch)
            throw CompileError(previousTok, "Invalid instruction 'fallthrough'" \
                " outside of match-is structure.");
        matchError(TOK_SEMICOLON, "Expect ';' after 'fallthrough'.");
        if (!checkTok(TOK_IS) && !checkTok(TOK_RIGHT_BRACE))
            throw CompileError(currentTok,
                "Cannot have a statement following a 'fallthrough' instruction.");
        fall = true;
        return nullptr;
    }
    else if (consumeTok(TOK_END))
    {
        if (!inMatch)
            throw CompileError(previousTok,
                "Invalid instruction 'end' outside of match-is structure.");
        matchError(TOK_SEMICOLON, "Expect ';' after 'end'.");
        return std::make_unique<EndStmt>();
    }
    return exprStmt();
}

StmtUP Parser::ifStmt()
{
    matchError(TOK_LEFT_PAREN, "Expect '(' after 'if'.");
    ExprUP condition = expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    StmtUP trueBranch = statement();
    StmtUP falseBranch = nullptr;
    if (consumeTok(TOK_ELIF))
        falseBranch = ifStmt();
    else if (consumeTok(TOK_ELSE))
        falseBranch = statement();
    return std::make_unique<IfStmt>(
        std::move(condition),
        std::move(trueBranch),
        std::move(falseBranch)
    );
}

StmtUP Parser::whileStmt()
{
    matchError(TOK_LEFT_PAREN, "Expect '(' after 'while'.");
    ExprUP condition = expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    return std::make_unique<WhileStmt>(std::move(condition),
        statement());
}

StmtUP Parser::matchStmt()
{
    bool prevInMatch = inMatch;
    inMatch = true;
    
    matchError(TOK_LEFT_PAREN, "Expect '(' before match value.");
    ExprUP match = expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after match value.");
    matchError(TOK_LEFT_BRACE, "Expect '{' before match cases.");

    std::vector<MatchStmt::matchCase> cases;
    cases.reserve(MATCH_CASES_MAX);

    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
    {
        if (static_cast<int>(cases.size()) == MATCH_CASES_MAX)
            throw CompileError(currentTok,
                "Too many cases in match-is structure.");
        
        matchError(TOK_IS, "Expect 'is' before case value.");
        ExprUP value;
        bool defaultCase = false;
        
        if (consumeTok(TOK_QMARK))
        {
            value = nullptr;
            defaultCase = true;
        }
        else
        {
            Token errorToken = currentTok;
            value = primary();
            if (value->type != E_LITERAL_EXPR)
                throw CompileError(errorToken,
                    "Case value must be a literal.");
        }

        matchError(TOK_COLON, "Expect ':' before case body.");
        StmtUP body;
        if (checkTok(TOK_IS) || checkTok(TOK_RIGHT_BRACE))
            body = nullptr;
        else
            body = statement();

        if (defaultCase && consumeTok(TOK_IS))
            throw CompileError(previousTok,
                "Cannot have another case after the default case.");

        cases.emplace_back(
            // 'fall' updated in statement().
            std::move(value), std::move(body), fall
        );
        fall = false; // Reset.
        if (defaultCase)
            break;
    }

    matchError(TOK_RIGHT_BRACE, "Expect '}' after match-is structure.");

    inMatch = prevInMatch;
    return std::make_unique<MatchStmt>(std::move(match), cases);
}

StmtUP Parser::repeatStmt()
{
    matchError(TOK_LEFT_BRACE, "Expect '{' before 'repeat' block.");
    StmtUP body = blockStmt(); // Will consume the '}'.
    matchError(TOK_UNTIL, "Expect 'until' condition after 'repeat'.");
    matchError(TOK_LEFT_PAREN, "Expect '(' before 'until' condition.");
    ExprUP condition = expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after 'until' condition.");
    matchError(TOK_SEMICOLON, "Expect ';' after repeat-until block.");

    return std::make_unique<RepeatStmt>(std::move(condition),
        std::move(body));
}

StmtUP Parser::blockStmt()
{
    StmtVec block;
    while (!checkTok(TOK_RIGHT_BRACE) && !checkTok(TOK_EOF))
        block.push_back(declaration());
    matchError(TOK_RIGHT_BRACE, "Expect '}' after block.");
    return std::make_unique<BlockStmt>(block);
}

StmtUP Parser::exprStmt()
{
    StmtUP ptr = std::make_unique<ExprStmt>(expression());
    matchError(TOK_SEMICOLON, "Expect ';' after expression.");
    return ptr;
}

ExprUP Parser::expression()
{
    return assignment();
}

ExprUP Parser::assignment()
{
    ExprUP target = logicOr();
    if (IS_ASSIGN(currentTok.type))
    {
        nextTok();
        Token oper = previousTok;
        if (target->type != E_VAR_EXPR) // Temporary.
            throw CompileError(previousTok, "Invalid assignment target.");
        target = std::make_unique<AssignExpr>(std::move(target),
            oper, expression());
    }
    return target;
}

ExprUP Parser::logicOr()
{
    ExprUP expr = logicAnd();
    while (consumeToks(TOK_BAR_BAR, TOK_OR))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<LogicExpr>(std::move(expr), oper,
            logicAnd());
    }

    return expr;
}

ExprUP Parser::logicAnd()
{
    ExprUP expr = equality();
    while (consumeToks(TOK_AMP_AMP, TOK_AND))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<LogicExpr>(std::move(expr), oper,
            equality());
    }

    return expr;
}

ExprUP Parser::equality()
{
    ExprUP expr = comparison();
    while (consumeToks(TOK_EQ_EQ, TOK_BANG_EQ))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<CompareExpr>(std::move(expr), oper,
            comparison());
    }

    return expr;
}

ExprUP Parser::comparison()
{
    ExprUP expr = bitOr();
    while (consumeToks(TOK_GT, TOK_GT_EQ, TOK_LT, TOK_LT_EQ))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<CompareExpr>(std::move(expr), oper,
            bitOr());
    }

    return expr;
}

ExprUP Parser::bitOr()
{
    ExprUP expr = bitXor();
    while (consumeTok(TOK_BAR))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<BitExpr>(std::move(expr), oper,
            bitXor());
    }

    return expr;
}

ExprUP Parser::bitXor()
{
    ExprUP expr = bitAnd();
    while (consumeTok(TOK_UARROW))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<BitExpr>(std::move(expr), oper,
            bitAnd());
    }

    return expr;
}

ExprUP Parser::bitAnd()
{
    ExprUP expr = shift();
    while (consumeTok(TOK_AMP))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<BitExpr>(std::move(expr), oper,
            shift());
    }

    return expr;
}

ExprUP Parser::shift()
{
    ExprUP expr = sum();
    while (consumeToks(TOK_RIGHT_SHIFT, TOK_LEFT_SHIFT))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<ShiftExpr>(std::move(expr), oper,
            shift());
    }

    return expr;
}

ExprUP Parser::sum()
{
    ExprUP expr = product();
    while (consumeToks(TOK_PLUS, TOK_MINUS))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<BinaryExpr>(std::move(expr), oper, 
            product());
    }

    return expr;
}

ExprUP Parser::product()
{
    ExprUP expr = unary();
    while (consumeToks(TOK_STAR, TOK_SLASH, TOK_PERCENT))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<BinaryExpr>(std::move(expr), oper, 
            unary());
    }

    return expr;
}

ExprUP Parser::unary()
{    
    if (consumeToks(TOK_INCR, TOK_DECR, TOK_MINUS,
        TOK_BANG, TOK_NOT, TOK_TILDE))
    {
        Token oper = previousTok;
        return ExprUP(std::make_unique<UnaryExpr>(oper, unary(), false));
    }

    return exponent();
}

ExprUP Parser::exponent()
{
    ExprUP expr = post();
    while (consumeTok(TOK_STAR_STAR))
    {
        TokenType oper = previousTok.type;
        expr = std::make_unique<BinaryExpr>(std::move(expr), oper,
            post());
    }

    return expr;
}

ExprUP Parser::post()
{
    ExprUP expr = primary();

    if (consumeToks(TOK_INCR, TOK_DECR))
    {
        if (expr->type != E_VAR_EXPR)
            throw CompileError(previousTok,
                "Invalid increment/decrement target.");
        do {
            Token oper = previousTok;
            expr = std::make_unique<UnaryExpr>(oper, std::move(expr), true);
        } while (consumeToks(TOK_INCR, TOK_DECR));
    }

    return expr;
}

ExprUP Parser::ifExpr()
{
    matchError(TOK_LEFT_PAREN, "Expect '(' before condition.");
    ExprUP condition = expression();
    matchError(TOK_RIGHT_PAREN, "Expect ')' after condition.");

    matchError(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
    ExprUP trueBranch = expression();
    matchError(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");

    ExprUP falseBranch = nullptr; // To avoid warnings.
    if (consumeTok(TOK_ELIF))
        falseBranch = ifExpr();
    else if (consumeTok(TOK_ELSE))
    {
        matchError(TOK_LEFT_BRACE, "Expect '{' before conditional expression.");
        falseBranch = expression();
        matchError(TOK_RIGHT_BRACE, "Expect '}' after conditional expression.");
    }
    else
        throw CompileError(currentTok,
            "A conditional expression must have a false-case branch.");

    return ExprUP(std::make_unique<IfExpr>(std::move(condition),
        std::move(trueBranch), std::move(falseBranch)));
}

ExprUP Parser::primary()
{
    nextTok();
    TokenType type = previousTok.type;

    if (IS_LITERAL(type))
        return ExprUP(std::make_unique<LiteralExpr>(previousTok));
    
    else if (type == TOK_IDENTIFIER)
        return ExprUP(std::make_unique<VarExpr>(previousTok));
    
    else if (type == TOK_LEFT_PAREN)
    {
        ExprUP expr = expression();
        matchError(TOK_RIGHT_PAREN, "Expect ')' after grouped expression.");
        return expr;
    }

    else if (type == TOK_IF)
        return ifExpr();
    
    return nullptr; // Temporary.
}

StmtVec& Parser::parseToAST(const vT& tokens)
{
    it = tokens.begin();
    currentTok = tokens[0];
    program.clear(); // In case we want to reuse the parser.

    try
    {
        while (!checkTok(TOK_EOF))
            program.push_back(declaration());
    }
    catch (CompileError& error)
    {
        error.report();
        program.clear(); // Temporarily.
        // reset();
    }

    return program;
}

#endif