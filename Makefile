CXX = g++
INCLUDES =	-Idependencies/personal \
			-Idependencies/fmt \
			-Idependencies/replxx -Idependencies/replxx/include
CXX_STANDARD = -std=c++17

# fmt: Compile as header-only library.
# replxx: Compile and link as static library (no DLL).
DEFINES = -D FMT_HEADER_ONLY -D REPLXX_STATIC
CH_ALLOCATOR = -D CH_USE_ALLOC=0 -D 'CH_ALLOC_SIZE=MiB(10)' -D CH_LINEAR_ALLOC
DEFINES += $(CH_ALLOCATOR)

DEBUG_FLAGS = -g -O0 -D DEBUG
RELEASE_FLAGS = -O2 -D NDEBUG
WARNINGS = -Wall -Wextra -Werror

# Prints out date and time (without time zone) of last commit.
COMMIT_TIME_STAMP = $(shell git log -1 --format=%ci | awk '{printf "%s %s\n", $$1, $$2}')
DEFINES += -D 'CH_COMMIT_TIME_STAMP="last modified: $(COMMIT_TIME_STAMP)"'

CXXFLAGS = $(INCLUDES) $(CXX_STANDARD) $(WARNINGS) $(DEFINES) -MMD -MP

REPL_DIR = dependencies/replxx
REPL_LIB = $(REPL_DIR)/libreplxx.a
LIBS = -L$(REPL_DIR) -lreplxx

AST = -D COMP_AST
TYPE = -D TYPE
OPT = -D OPT

NAME = choice
RELEASE = choice-release
DEBUG = choice-debug
SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# Testing.

TEST_DIR = test
TEST_COUNT = $(shell find ${TEST_DIR} -type f | grep .ch | wc -l)
PYTHON = python3
PY_TEST_FILE = run_tests.py
TEST_QUIET = off

ifeq ($(TEST_QUIET),off)
	TEST_CMD = $(PYTHON) $(TEST_DIR)/$(PY_TEST_FILE)
else
	TEST_CMD = $(PYTHON) $(TEST_DIR)/$(PY_TEST_FILE) --quiet
endif

all: $(NAME)

$(NAME): $(OBJS) $(REPL_LIB)
	@$(CXX) $(CXXFLAGS) $^ $(LIBS) -o $(NAME)

ast: CXXFLAGS += $(AST)
ast: $(NAME)

type: CXXFLAGS += $(AST) $(TYPE)
type: $(NAME)

opt: $(CXXFLAGS) += $(AST) $(OPT)
opt: $(NAME)

# Assuming only AST version with this Makefile (for now).

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: NAME = $(DEBUG)
debug: ast

release: CXXFLAGS += $(RELEASE_FLAGS)
release: NAME = $(RELEASE)
release: ast

test:
	@echo "Running $(TEST_COUNT) tests...\n"
	@$(TEST_CMD)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(REPL_LIB):
	@make --no-print-directory -C $(REPL_DIR)

clean:
	@rm -f $(OBJ_DIR)/*.o
	@rm -f $(OBJ_DIR)/*.d
	@make --no-print-directory -C $(REPL_DIR) clean

fclean: clean
	@rm -f $(NAME)
	@rm -f $(DEBUG)
	@rm -f $(REPL_LIB)

re: fclean all

-include $(OBJS:.o=.d)

.PHONY: all ast type opt debug release test clean fclean re