CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Werror -pedantic
ALT = -DALT


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

$(OBJ_DIR):
ifeq ($(PLATFORM), windows)
	@powershell -Command "if (-not (Test-Path $(OBJ_DIR))) \
	{[void](New-Item -ItemType Directory -Path $(OBJ_DIR))}"
else
	@mkdir -p $(OBJ_DIR)
endif

$(NAME): $(OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) $(ALT) -c $< -o $@

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