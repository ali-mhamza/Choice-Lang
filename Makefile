CXX = g++
# Includes for my linear probing hash table implementation (found on GitHub),
# as well as the libfmt and replxx libraries.
INCLUDES = -Idependencies/personal \
			-Idependencies/fmt \
			-Idependencies/replxx -Idependencies/replxx/include
CXX_STANDARD = -std=c++17
DEFINES = -DFMT_HEADER_ONLY
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG
WARNINGS = -Wall -Wextra \
			-Wno-unused-parameter -Wno-sign-compare -Wno-maybe-uninitialized -Wno-unused-label \
			-Wno-error=pedantic -Werror
CXXFLAGS = $(INCLUDES) $(CXX_STANDARD) $(RELEASE_FLAGS) $(WARNINGS) $(DEFINES)

REPL_DIR = dependencies/replxx
REPL_LIB = $(REPL_DIR)/libreplxx.a
LIBS = -L$(REPL_DIR) -lreplxx

AST = -DCOMP_AST
TYPE = -DTYPE
OPT = -DOPT

NAME = choice
SRC_DIR = src
INCLUDE_DIR = include
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

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/main.o: $(SRC_DIR)/main.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INCLUDE_DIR)/%.h | $(OBJ_DIR)
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

.PHONY: all clean fclean re