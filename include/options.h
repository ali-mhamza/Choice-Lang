#pragma once
#include "common.h"
#include <string_view>

void optionExecute(FileID id, std::string_view input);
void optionEmitTokens(FileID id, std::string_view input);
void optionEmitBytecode(FileID id, std::string_view input);
void optionCacheBytecode(FileID id, std::string_view input);
void optionDisProgram(FileID id, std::string_view input);
void optionLoadProgram(FileID id, std::string_view input);
void optionCheckProgram(FileID id, std::string_view input);
void optionInspectBytecode(FileID id, std::string_view input);
void optionRunTests(FileID id, std::string_view input);
void optionExplainError(FileID id, std::string_view input);