CXX = g++
CXXFLAGS = -Wall -Wextra -Werror

NAME = choice
SRC_DIR = src
OBJ_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

all: $(NAME)

$(NAME): $(OBJS)
	@mkdir $(OBJ_DIR)
	@CXX CXXFLAGS -o $(OBJ_DIR)/${choice}

%.o: %.cpp
	@mkdir $(OBJ_DIR)
	@CXX CXXFLAGS -c $< -o $(OBJ_DIR)/$@

clean:
	@rm -rf $(OBJ_DIR)/*.o

fclean: clean
	@rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re