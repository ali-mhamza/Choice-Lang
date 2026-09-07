#include "../include/ast_printer.h"
#include "../include/utils.h"

using namespace AST::Types;
using namespace AST::Statement;
using namespace AST::Expression;

#define DEF(type) void ASTPrinter::print##type(const type* node)

#define PRINT(type)                                             \
    do {                                                        \
        const auto* ptr{static_cast<const type*>(node.get())};  \
        print##type(ptr);                                       \
    } while (false)

#define PRINT_LAMBDA(func)                  \
    [this](const auto& entry, bool last) {  \
        func(entry, last);                  \
    }

#define PRINT_OPERATOR(type)                                \
    do {                                                    \
        printName(type, !ADD_NEWLINE);                      \
        CH_PRINT(" '{}'\n", operatorString(node->oper));    \
        printExpr(node->left, !LAST_CHILD);                 \
        printExpr(node->right, LAST_CHILD);                 \
    } while (false)

constexpr bool LAST_CHILD{true};
constexpr bool ADD_NEWLINE{true};

void ASTPrinter::beginChild(std::string_view name, bool last, bool newline)
{
    lastChildren.push_back(last);
    printName(name, newline);
}

void ASTPrinter::endChild()
{
    exitNode();
}

void ASTPrinter::printDepth()
{
    for (u64 i{1}; i < lastChildren.size(); i++)
    {
        if (lastChildren[i - 1])
            CH_PRINT("    ");
        else
            CH_PRINT("{}   ", VERTICAL_BAR);
    }
}

void ASTPrinter::printName(std::string_view name, bool newline)
{
    printDepth();
    CH_PRINT("{}", lastChildren.back() ? LEFT_CORNER : LEFT_BRANCH);
    CH_PRINT("{}{}", name, (newline ? "\n" : ""));
}

void ASTPrinter::printVar(const AST::Var& print, bool last)
{
    beginChild("", last, false);
    CH_PRINT("'{}': ", print.var.text);
    printTypeHint(print.typeHint);
    CH_PRINT("\n");
    endChild();
}

void ASTPrinter::printParam(const AST::Param& print, bool last)
{
    beginChild("", last, !ADD_NEWLINE);
    CH_PRINT("'{}{}'", print.param.var.text, print.variadic ? "..." : "");
    if (print.fix) CH_PRINT(" [fix]");
    CH_PRINT(": ");
    printTypeHint(print.param.typeHint);
    CH_PRINT("\n");

    if (print.defaultVal != nullptr)
    {
        beginChild("Default", LAST_CHILD, ADD_NEWLINE);
        printExpr(print.defaultVal, true);
        endChild();
    }

    endChild();
}

void ASTPrinter::printLoopHeader(const AST::LoopHeader& header)
{
    beginChild("Vars", !LAST_CHILD, !ADD_NEWLINE);
    CH_PRINT("{}\n", header.fix ? " [fix]" : "");
    forEachChild(header.vars, PRINT_LAMBDA(printVar));
    endChild();

    // Still 'beginChild' even if there is no 'where' clause
    // or unpack state to display, since the body of the loop
    // always follows.
    beginChild("Iter", !LAST_CHILD, ADD_NEWLINE);
    printExpr(header.iter, true);
    endChild();

    if (header.where != nullptr)
    {
        beginChild("Where", !LAST_CHILD, ADD_NEWLINE);
        printExpr(header.where, true);
        endChild();
    }

    printUnpackState(header.unpack, !LAST_CHILD);
}

void ASTPrinter::printBody(
    const StmtUP& body,
    bool last,
    std::string_view name
)
{
    beginChild(name, last, ADD_NEWLINE);

    auto empty = [this] {
        beginChild("[EMPTY]", LAST_CHILD, ADD_NEWLINE);
        endChild();
    };

    if (body == nullptr)
        empty();
    else if (body->type == StmtType::BlockStmt)
    {
        const BlockStmt* stmt{static_cast<const BlockStmt*>(body.get())};
        if (stmt->block.empty())
            empty();
        else
            forEachChild(stmt->block, PRINT_LAMBDA(printStmt));
    }
    else
        printStmt(body, true);

    endChild();
}

void ASTPrinter::printDecl(const AST::Decl& decl, bool last)
{
    if (decl.attr != static_cast<VarAttr>(0))
    {
        beginChild("Attrs", last, !ADD_NEWLINE);
        CH_PRINT(": [");

        bool first{true};
        for (const auto& tok : decl.attrTokens)
        {
            if (tok)
            {
                if (first)
                    first = false;
                else
                    CH_PRINT(", ");

                CH_PRINT("{}", tok.text);
            }
        }
        CH_PRINT("]\n");
        endChild();
    }
}

void ASTPrinter::printUnpackState(const AST::UnpackState& state, bool last)
{
    if (!state) return;
    beginChild("Unpack", last, !ADD_NEWLINE);
    CH_PRINT(": {}\n", (state.unpackIgnore ? "ignore" : "last"));
    endChild();
}

void ASTPrinter::printSimpleType(const TypeHint& hint)
{
    const auto& tok{std::get<Token>(hint.variant)};
    CH_PRINT("{}", tok.text);
}

void ASTPrinter::printSignatureType(const TypeHint& hint)
{
    CH_PRINT("Callable(");
    const auto& signature{std::get<Signature>(hint.variant)};
    bool first{true};

    for (const auto& param : signature.paramTypes)
    {
        if (first)
            first = false;
        else
            CH_PRINT(", ");
        printTypeHint(param);
    }

    CH_PRINT(") -> ");
    printTypeHint(signature.returnType);
}

void ASTPrinter::printOptionType(const TypeHint& hint)
{
    CH_PRINT("<");
    const auto& options{std::get<TypeVec>(hint.variant)};
    bool first{true};

    for (const auto& option : options)
    {
        if (first)
            first = false;
        else
            CH_PRINT(" | ");
        printTypeHint(option);
    }

    CH_PRINT(">");
}

void ASTPrinter::printGroupType(const TypeHint& hint)
{
    CH_PRINT("(");
    const auto& options{std::get<TypeVec>(hint.variant)};
    bool first{true};

    for (const auto& option : options)
    {
        if (first)
            first = false;
        else
            CH_PRINT(", ");
        printTypeHint(option);
    }

    CH_PRINT(")");
}

void ASTPrinter::printGenericType(const TypeHint& hint)
{
    const auto& generic{std::get<Generic>(hint.variant)};
    printTypeHint(generic.baseType);

    bool first{true};
    CH_PRINT("[");
    for (const auto& spec : generic.specTypes)
    {
        if (first)
            first = false;
        else
            CH_PRINT(", ");
        printTypeHint(spec);
    }
    CH_PRINT("]");
}

void ASTPrinter::printRefType(const TypeHint& hint)
{
    CH_PRINT("*");
    printTypeHint(std::get<TypeUP>(hint.variant));
}

void ASTPrinter::printNullableType(const TypeHint& hint)
{
    printTypeHint(std::get<TypeUP>(hint.variant));
    CH_PRINT("?");
}

void ASTPrinter::printTypeHint(const TypeUP& type)
{
    if (type == nullptr)
        CH_PRINT("()");
    else
        printTypeHint(*type);
}

void ASTPrinter::printTypeHint(const TypeHint& hint)
{
    switch (hint.tag)
    {
        case HintType::Dummy:       CH_PRINT("()");             break;
        case HintType::Any:         CH_PRINT("Any");            break;
        case HintType::Name:        printSimpleType(hint);      break;
        case HintType::Signature:   printSignatureType(hint);   break;
        case HintType::Option:      printOptionType(hint);      break;
        case HintType::Group:       printGroupType(hint);       break;
        case HintType::Generic:     printGenericType(hint);     break;
        case HintType::Reference:   printRefType(hint);         break;
        case HintType::Nullable:    printNullableType(hint);    break;
    }
}

/* Statement printers. */

DEF(VarDecl)
{
    printName("VarDecl", !ADD_NEWLINE);
    CH_PRINT("{}\n", node->fix ? " [fix]" : "");
    printDecl(node->decl, !LAST_CHILD);

    beginChild("Names", !LAST_CHILD, ADD_NEWLINE);
    forEachChild(node->names, PRINT_LAMBDA(printVar));
    endChild();

    beginChild("Values", !node->unpack, ADD_NEWLINE);
    forEachChild(node->values, PRINT_LAMBDA(printExpr));
    endChild();

    printUnpackState(node->unpack, LAST_CHILD);
}

DEF(FuncDecl)
{
    printName("FuncDecl", !ADD_NEWLINE);
    CH_PRINT(" '{}' -> ", node->name.text);
    printTypeHint(node->typeHint);
    CH_PRINT("\n");
    printDecl(node->decl, !LAST_CHILD);

    if (!node->params.empty())
    {
        beginChild("Params", !LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->params, PRINT_LAMBDA(printParam));
        endChild();
    }

    printBody(node->body, true);
}

DEF(TypeDecl)
{
    auto printField = [this](const TypeDecl::Field& field, bool last) {
        beginChild("Field", last, !ADD_NEWLINE);
        CH_PRINT(" '{}'", field.name.var.text);
        CH_PRINT("{}: ", field.fix ? " [fix]" : "");
        printTypeHint(field.name.typeHint);
        CH_PRINT("\n");
        printDecl(field.decl, field.init == nullptr);

        if (field.init != nullptr)
        {
            CH_ASSERT(
                field.init->type == ExprType::LambdaExpr,
                "Field initializer is not parsed as a lambda."
            );

            beginChild("Init", LAST_CHILD, ADD_NEWLINE);
            const LambdaExpr* lambda{static_cast<const LambdaExpr*>(field.init.get())};
            if (lambda->body == nullptr)
                printExpr(nullptr, true);
            else
            {
                CH_ASSERT(
                    lambda->body->type == StmtType::ReturnStmt,
                    "Field initializer 'body' is not a return statement."
                );
                const ReturnStmt* stmt{static_cast<const ReturnStmt*>(lambda->body.get())};
                printExpr(stmt->expr, true);
            }
            endChild();
        }

        endChild();
    };

    printName("TypeDecl", !ADD_NEWLINE);
    CH_PRINT(" '{}'\n", node->name.text);
    bool hasFields{!node->fields.empty()}, hasMethods{!node->methods.empty()};
    printDecl(node->decl, !hasFields && !hasMethods);

    if (hasFields)
    {
        if (hasMethods)
        {
            for (const auto& field : node->fields)
                printField(field, false);
        }
        else
            forEachChild(node->fields, printField);
    }

    if (hasMethods)
    {
        beginChild("Methods", LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->methods, PRINT_LAMBDA(printStmt));
        endChild();
    }
}

DEF(AliasDecl)
{
    printName("AliasDecl", !ADD_NEWLINE);
    CH_PRINT(" '{}' = ", node->alias.text);
    printTypeHint(node->hint);
    CH_PRINT("\n");
}

DEF(UseStmt)
{
    auto printAliasedName = [](const Token& name, const Token& alias) {
        if (alias)
            CH_PRINT(" '{}' (=> '{}')\n", name.text, alias.text);
        else
            CH_PRINT(" '{}'\n", name.text);
    };

    printName("Use");
    bool hasDirectory{node->directory};
    bool hasEntries{!node->entries.empty()};
    bool onlyModule{!hasDirectory && !hasEntries};

    beginChild("Module", onlyModule, !ADD_NEWLINE);
    printAliasedName(node->module, node->alias);
    endChild();

    if (node->directory)
    {
        beginChild("Directory", !hasEntries, !ADD_NEWLINE);
        const auto& text{node->directory.text};
        CH_PRINT(" '{}'\n", text.substr(1, text.size() - 2));
        endChild();
    }

    forEachChild(node->entries, [&](const auto& entry, bool last) {
        beginChild("Entry", last, !ADD_NEWLINE);
        printAliasedName(entry.name, entry.alias);
        endChild();
    });
}

DEF(IfStmt)
{
    printName("If");

    beginChild("Condition", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->condition, true);
    endChild();

    printBody(node->trueBranch, (node->falseBranch == nullptr), "Then");
    if (node->falseBranch != nullptr)
        printBody(node->falseBranch, true, "Else");
}

DEF(WhileStmt)
{
    printName("While", !ADD_NEWLINE);
    if (node->label) CH_PRINT(" '{}'", node->label.text);
    CH_PRINT("\n");

    beginChild("Condition", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->condition, true);
    endChild();

    printBody(node->body, (node->elseClause == nullptr));
    if (node->elseClause != nullptr)
        printBody(node->elseClause, true, "Else");
}

DEF(ForStmt)
{
    printName("For", !ADD_NEWLINE);
    if (node->label) CH_PRINT(" '{}'", node->label.text);
    CH_PRINT("\n");
    printLoopHeader(node->header);

    printBody(node->body, (node->elseClause == nullptr));
    if (node->elseClause != nullptr)
        printBody(node->elseClause, true, "Else");
}

DEF(MatchStmt)
{
    auto printCase = [this](const MatchStmt::MatchCase& matchCase, bool last) {
        beginChild("Case", last, !ADD_NEWLINE);
        CH_PRINT("{}\n", matchCase.fallthrough ? " [fallthrough]" : "");

        printExpr(matchCase.value, false);
        printBody(matchCase.body, true);

        endChild();
    };

    printName("Match");

    beginChild("Value", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->matchValue, true);
    endChild();

    beginChild("Cases", LAST_CHILD, ADD_NEWLINE);
    forEachChild(node->cases, printCase);
    endChild();
}

DEF(RepeatStmt)
{
    printName("Repeat", !ADD_NEWLINE);
    if (node->label) CH_PRINT(" '{}'", node->label.text);
    CH_PRINT("\n");

    beginChild("Condition", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->condition, true);
    endChild();

    printBody(node->body, true);
}

DEF(ReturnStmt)
{
    printName("Return");
    printExpr(node->expr, true);
}

DEF(BreakStmt)
{
    printName("Break", !ADD_NEWLINE);
    if (node->label)
        CH_PRINT(" '{}'", node->label.text);
    CH_PRINT("\n");
}

DEF(ContinueStmt)
{
    printName("Continue", !ADD_NEWLINE);
    if (node->label)
        CH_PRINT(" '{}'", node->label.text);
    CH_PRINT("\n");
}

DEF(EndStmt)
{
    (void) node;

    printName("End");
}

DEF(ExprStmt)
{
    // To counteract the increment in printStmt(), since
    // we are effectively trying to replace the ExprStmt node
    // with its contained expression.

    bool state{lastChildren.back()};
    exitNode();
    printExpr(node->expr, state);
    enterNode(state);
}

DEF(BlockStmt)
{
    printName("Block");
    forEachChild(node->block, PRINT_LAMBDA(printStmt));
}

/* Expression printers. */

DEF(MutExpr)
{
    printName((node->mut ? "Mut" : "Immut"));
    printExpr(node->value, true);
}

DEF(AssignExpr)
{
    printName("Assign", !ADD_NEWLINE);
    if (node->oper.type != TOK_EQUAL)
        CH_PRINT(" '{}'", node->oper.text);
    CH_PRINT("\n");

    beginChild("Targets", !LAST_CHILD, ADD_NEWLINE);
    forEachChild(node->targets, PRINT_LAMBDA(printExpr));
    endChild();

    beginChild("Values", !node->unpack, ADD_NEWLINE);
    forEachChild(node->values, PRINT_LAMBDA(printExpr));
    endChild();

    printUnpackState(node->unpack, LAST_CHILD);
}

static constexpr std::string_view operatorString(TokenType op)
{
    switch (op)
    {
        // Logical operators.
        case TOK_BAR_BAR:       return "||";
        case TOK_OR:            return "or";
        case TOK_AMP_AMP:       return "&&";
        case TOK_AND:           return "and";

        // Comparison operators.
        case TOK_EQ_EQ:         return "==";
        case TOK_BANG_EQ:       return "!=";
        case TOK_GT:            return ">";
        case TOK_GT_EQ:         return ">=";
        case TOK_LT:            return "<";
        case TOK_LT_EQ:         return "<=";
        case TOK_IN:            return "in";
        case TOK_NOT:           return "not in";

        // Bitwise operators.
        case TOK_BAR:           return "|";
        case TOK_UARROW:        return "^";
        case TOK_AMP:           return "&";

        // Bit-shift operators.
        case TOK_RIGHT_SHIFT:   return ">>";
        case TOK_LEFT_SHIFT:    return "<<";

        // Binary operators.
        case TOK_DOT_DOT_EQ:    return "..=";
        case TOK_DOT_DOT_LT:    return "..<";
        case TOK_PLUS:          return "+";
        case TOK_MINUS:         return "-";
        case TOK_STAR:          return "*";
        case TOK_SLASH:         return "/";
        case TOK_PERCENT:       return "%";
        case TOK_STAR_STAR:     return "**";

        default: CH_UNREACHABLE();
    }
}

DEF(LogicExpr)
{
    PRINT_OPERATOR("Logic");
}

DEF(CompareExpr)
{
    PRINT_OPERATOR("Compare");
}

DEF(BitExpr)
{
    PRINT_OPERATOR("Bit");
}

DEF(ShiftExpr)
{
    PRINT_OPERATOR("Shift");
}

DEF(BinaryExpr)
{
    PRINT_OPERATOR("Binary");
}

DEF(UnaryExpr)
{
    printName("Unary", !ADD_NEWLINE);
    CH_PRINT(" '{}'", node->oper.text);
    // node->prev = Evaluate to previous value (post-increment).
    CH_PRINT("{}\n", (node->prev ? " [post]" : ""));
    printExpr(node->expr, true);
}

DEF(IndexExpr)
{
    printName("Index");
    printExpr(node->obj, false);
    printExpr(node->index, true);
}

DEF(CallExpr)
{
    printName("Call", !ADD_NEWLINE);
    CH_PRINT("{}\n", (node->builtin ? " [builtin]" : ""));
    bool hasArgs{!node->args.empty()};

    beginChild("Callee", !hasArgs, !ADD_NEWLINE);
    if ((node->callee != nullptr) && (node->callee->type == ExprType::VarExpr))
    {
        const VarExpr* var{static_cast<const VarExpr*>(node->callee.get())};
        CH_PRINT(": '{}'\n", var->name.text);
    }
    else
    {
        CH_PRINT("\n");
        printExpr(node->callee, true);
    }
    endChild();

    if (hasArgs)
    {
        beginChild("Args", LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->args, PRINT_LAMBDA(printExpr));
        endChild();
    }
}

DEF(FieldExpr)
{
    printName("Field", !ADD_NEWLINE);
    CH_PRINT(" '.{}'\n", node->field.text);
    printExpr(node->obj, true);
}

DEF(ScopeExpr)
{
    printName("Scope", !ADD_NEWLINE);
    CH_PRINT(" '::{}'\n", node->entry.text);
    printExpr(node->module, true);
}

DEF(IfExpr)
{
    printName("Conditional");

    beginChild("Condition", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->condition, true);
    endChild();

    beginChild("Then", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->trueExpr, true);
    endChild();

    // If-expressions must have false-case branches,
    // unlike if-statements.
    beginChild("Else", LAST_CHILD, ADD_NEWLINE);
    printExpr(node->falseExpr, true);
    endChild();
}

DEF(LambdaExpr)
{
    printName("Lambda", !ADD_NEWLINE);
    CH_PRINT("{} -> ", node->iife ? " [IIFE]" : "");
    printTypeHint(node->typeHint);
    CH_PRINT("\n");

    if (!node->params.empty())
    {
        beginChild("Params", !LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->params, PRINT_LAMBDA(printParam));
        endChild();
    }

    printBody(node->body, true);
}

DEF(ListExpr)
{
    printName("List");
    if (!node->entries.empty())
    {
        beginChild("Entries", LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->entries, PRINT_LAMBDA(printExpr));
        endChild();
    }
}

DEF(TableExpr)
{
    printName("Table");
    if (!node->pairs.empty())
    {
        beginChild("Pairs", LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->pairs, [this](const auto& entry, bool last) {
            beginChild("Pair", last, ADD_NEWLINE);

            beginChild("Key", !LAST_CHILD, ADD_NEWLINE);
            printExpr(entry.key, true);
            endChild();

            beginChild("Value", LAST_CHILD, ADD_NEWLINE);
            printExpr(entry.value, true);
            endChild();

            endChild();
        });
        endChild();
    }
}

DEF(InstanceExpr)
{
    printName("Instance");
    bool hasFields{!node->fields.empty()};

    beginChild("Type", !hasFields, ADD_NEWLINE);
    printExpr(node->type, true);
    endChild();

    if (hasFields)
    {
        beginChild("Fields", LAST_CHILD, ADD_NEWLINE);
        forEachChild(node->fields, [this](const auto& entry, bool last) {
            beginChild("", last, !ADD_NEWLINE);
            CH_PRINT("'{}'\n", entry.name.text);
            printExpr(entry.init, true);
            endChild();
        });
        endChild();
    }
}

DEF(ListCompExpr)
{
    printName("List Comprehension");
    printLoopHeader(node->header);

    beginChild("Entry", LAST_CHILD, ADD_NEWLINE);
    printExpr(node->expr, true);
    endChild();
}

DEF(TableCompExpr)
{
    printName("Table Comprehension");
    printLoopHeader(node->header);

    beginChild("Key", !LAST_CHILD, ADD_NEWLINE);
    printExpr(node->key, true);
    endChild();

    beginChild("Value", LAST_CHILD, ADD_NEWLINE);
    printExpr(node->value, true);
    endChild();
}

DEF(RefExpr)
{
    printName("Ref");
    printExpr(node->obj, true);
}

DEF(VarExpr)
{
    printName("Var", !ADD_NEWLINE);
    CH_PRINT(" '{}'\n", node->name.text);
}

DEF(StringPartExpr)
{
    printName("String Part", !ADD_NEWLINE);
    CH_PRINT(" '{}'\n", node->part.text);
}

DEF(FormatExpr)
{
    printName("Format");

    beginChild("Parts", LAST_CHILD, ADD_NEWLINE);
    forEachChild(node->parts, PRINT_LAMBDA(printExpr));
    endChild();
}

DEF(LiteralExpr)
{
    printName("Literal", !ADD_NEWLINE);
    std::string_view text{node->value.text};

    // In case of potentially synthetic tokens.
    if (text.empty())
    {
        switch (node->value.type)
        {
            case TOK_TRUE:  text = "true";  break;
            case TOK_FALSE: text = "false"; break;
            case TOK_NULL:  text = "null";  break;
            default: CH_UNREACHABLE();
        }
    }

    CH_PRINT(" {}\n", text);
}

/* General driver functions. */

void ASTPrinter::printExpr(const ExprUP& node, bool last)
{
    #define CASE(type) case ExprType::type

    enterNode(last);

    if (node == nullptr)
    {
        printName("[EMPTY]");
        exitNode();
        return;
    }

    switch (node->type)
    {
        CASE(MutExpr):          PRINT(MutExpr);         break;
        CASE(AssignExpr):       PRINT(AssignExpr);      break;
        CASE(LogicExpr):        PRINT(LogicExpr);       break;
        CASE(CompareExpr):      PRINT(CompareExpr);     break;
        CASE(BitExpr):          PRINT(BitExpr);         break;
        CASE(ShiftExpr):        PRINT(ShiftExpr);       break;
        CASE(BinaryExpr):       PRINT(BinaryExpr);      break;
        CASE(UnaryExpr):        PRINT(UnaryExpr);       break;
        CASE(IndexExpr):        PRINT(IndexExpr);       break;
        CASE(CallExpr):         PRINT(CallExpr);        break;
        CASE(FieldExpr):        PRINT(FieldExpr);       break;
        CASE(ScopeExpr):        PRINT(ScopeExpr);       break;
        CASE(IfExpr):           PRINT(IfExpr);          break;
        CASE(LambdaExpr):       PRINT(LambdaExpr);      break;
        CASE(ListExpr):         PRINT(ListExpr);        break;
        CASE(TableExpr):        PRINT(TableExpr);       break;
        CASE(InstanceExpr):     PRINT(InstanceExpr);    break;
        CASE(ListCompExpr):     PRINT(ListCompExpr);    break;
        CASE(TableCompExpr):    PRINT(TableCompExpr);   break;
        CASE(RefExpr):          PRINT(RefExpr);         break;
        CASE(VarExpr):          PRINT(VarExpr);         break;
        CASE(StringPartExpr):   PRINT(StringPartExpr);  break;
        CASE(FormatExpr):       PRINT(FormatExpr);      break;
        CASE(LiteralExpr):      PRINT(LiteralExpr);     break;
    }

    exitNode();

    #undef CASE
}

void ASTPrinter::printStmt(const StmtUP& node, bool last)
{
    #define CASE(type) case StmtType::type

    enterNode(last);

    if (node == nullptr)
    {
        printName("[EMPTY]");
        exitNode();
        return;
    }

    switch (node->type)
    {
        CASE(VarDecl):      PRINT(VarDecl);         break;
        CASE(FuncDecl):     PRINT(FuncDecl);        break;
        CASE(TypeDecl):     PRINT(TypeDecl);        break;
        CASE(AliasDecl):    PRINT(AliasDecl);       break;
        CASE(UseStmt):      PRINT(UseStmt);         break;
        CASE(IfStmt):       PRINT(IfStmt);          break;
        CASE(WhileStmt):    PRINT(WhileStmt);       break;
        CASE(ForStmt):      PRINT(ForStmt);         break;
        CASE(MatchStmt):    PRINT(MatchStmt);       break;
        CASE(RepeatStmt):   PRINT(RepeatStmt);      break;
        CASE(ReturnStmt):   PRINT(ReturnStmt);      break;
        CASE(BreakStmt):    PRINT(BreakStmt);       break;
        CASE(ContinueStmt): PRINT(ContinueStmt);    break;
        CASE(EndStmt):      PRINT(EndStmt);         break;
        CASE(ExprStmt):     PRINT(ExprStmt);        break;
        CASE(BlockStmt):    PRINT(BlockStmt);       break;
    }

    exitNode();

    #undef CASE
}

void ASTPrinter::printAST(const StmtVec& program)
{
    if (!program.empty())
    {
        bool printSpace{false};

        CH_PRINT("Program\n");
        for (const StmtUP& stmt : program)
        {
            if (printSpace)
                CH_PRINT("{}\n", VERTICAL_BAR);
            else
                printSpace = true;

            printStmt(stmt, &stmt == &(program.back()));
        }

        CH_PRINT("\n");
    }
}

#undef DEF
#undef PRINT
#undef PRINT_LAMBDA
#undef PRINT_OPERATOR