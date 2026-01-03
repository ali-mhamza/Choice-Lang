include .env

CXX = g++
# Includes for my separate chaining hash table implementation (found on GitHub),
# as well as the libfmt library.
INCLUDES = $(INCLUDE) $(INCLUDE_WSL)
CXX_STANDARD = -std=c++17
DEFINES = -DFMT_HEADER_ONLY
# DEBUG_FLAGS = -g -O0 -DDEBUG
# RELEASE_FLAGS = -g -O2 -DNDEBUG
REGULAR_FLAGS = -g -O2
WARNINGS = -Wall -Wextra \
			-Wno-unused-parameter -Wno-sign-compare -Wno-maybe-uninitialized\
			-Werror -pedantic
CXXFLAGS = $(INCLUDES) $(CXX_STANDARD) $(REGULAR_FLAGS) $(WARNINGS) $(DEFINES)
AST = -DCOMP_AST
TYPE = -DTYPE
OPT = -DOPT

ifeq ($(OS), Windows_NT)
    PLATFORM = windows
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S), Linux)
        PLATFORM = linux
    endif
    ifeq ($(UNAME_S), Darwin)
        PLATFORM = macos
    endif
endif

NAME = choice
SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(NAME)
ifeq ($(PLATFORM), windows)
# Open Powershell to run the executable in a proper window.
	@start powershell
endif

$(OBJ_DIR):
ifeq ($(PLATFORM), windows)
	@powershell -Command "if (-not (Test-Path $(OBJ_DIR))) \
	{[void](New-Item -ItemType Directory -Path $(OBJ_DIR))}"
else
	@mkdir -p $(OBJ_DIR)
endif

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $(NAME)

ast: CXXFLAGS += $(AST)
ast: $(NAME)

type: CXXFLAGS += $(AST) $(TYPE)
type: $(NAME)

opt: $(CXXFLAGS) += $(AST) $(OPT)
opt: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
ifeq ($(PLATFORM), windows)
	@powershell -Command "if (Test-Path $(OBJ_DIR)) \
	{[void](Remove-Item -Recurse -Force $(OBJ_DIR)/*.o)}"
else
	@rm -f $(OBJ_DIR)/*.o
endif

fclean: clean
ifeq ($(PLATFORM), windows)
	@powershell -Command "if (Test-Path $(NAME)) \
	{[void](Remove-Item -Force $(NAME))}"
else
	@rm -f $(NAME)
endif

re: fclean all

.PHONY: all clean fclean re