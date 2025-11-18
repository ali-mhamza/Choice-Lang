#pragma once

class Error {};
class LexError : public Error {};
class CompileError : public Error {};
class RuntimeError : public Error {};
class TypeError : public Error {}; // For static type-checking.