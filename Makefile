CXX = g++
INCLUDES = -Idependencies/personal \
			-Idependencies/fmt \
			-Idependencies/replxx -Idependencies/replxx/include
CXX_STANDARD = -std=c++17
DEFINES = -D FMT_HEADER_ONLY -D USE_ALLOC=0 # -D LINEAR_ALLOC
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG
WARNINGS = -Wall -Wextra \
			-Wno-unused-parameter -Wno-sign-compare -Wno-maybe-uninitialized -Wno-unused-label \
			-Wno-error=pedantic -Werror \
			-MMD -MP
CXXFLAGS = $(INCLUDES) $(CXX_STANDARD) $(WARNINGS) $(DEFINES)

REPL_DIR = dependencies/replxx
REPL_LIB = $(REPL_DIR)/libreplxx.a
LIBS = -L$(REPL_DIR) -lreplxx

AST = -DCOMP_AST
TYPE = -DTYPE
OPT = -DOPT

NAME = choice
RELEASE = choice-release
DEBUG = choice-debug
SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

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

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(REPL_LIB):
	@make --no-print-directory -C $(REPL_DIR)

clean:
	@rm -f $(OBJ_DIR)/*.o
	@make --no-print-directory -C $(REPL_DIR) clean

fclean: clean
	@rm -f $(NAME)
	@rm -f $(REPL_LIB)

re: fclean all

-include $(OBJS:.o=.d)

.PHONY: all clean fclean re