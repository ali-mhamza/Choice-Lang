# Makefile done with help from ChatGPT.

CXX = g++
CXXFLAGS = -std=c++17 -O2 
DEFINES = -D REPLXX_STATIC -D NDEBUG
INCLUDE = -Isrc -Iinclude

BUILD_DIR = build
AR = ar rcs
LIB = libreplxx.a

SRCS := $(wildcard src/*.cpp) $(wildcard src/*.cxx)
OBJS := $(patsubst src/%, $(BUILD_DIR)/%, $(SRCS))
OBJS := $(OBJS:.cpp=.o)
OBJS := $(OBJS:.cxx=.o)

all: $(LIB)

$(LIB): $(OBJS)
	@$(AR) $@ $^

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDE) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.cxx | $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDE) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)/*.o
