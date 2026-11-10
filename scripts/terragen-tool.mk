# terragen built once for the standalone tools (scripts/terragen-tool.sh):
# objects kept in OBJ, rebuilt only as their sources or headers change
SRC := $(ROOT)/apps/game/src
TERRAGEN := $(wildcard $(SRC)/terragen/*.cpp)
OBJS := $(patsubst $(SRC)/terragen/%.cpp,$(OBJ)/%.o,$(TERRAGEN))
CXXFLAGS := -std=c++17 -O2 -I$(SRC) -MMD -MP

$(OUT): $(OBJS) $(TOOL)
	$(CXX) $(CXXFLAGS) $(TOOL) $(OBJS) -o $@ -lpthread

$(OBJ)/%.o: $(SRC)/terragen/%.cpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)
