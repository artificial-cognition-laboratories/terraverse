# terragen built once for the standalone tools (scripts/terragen-tool.sh):
# objects kept in OBJ, rebuilt only as their sources or headers change
GAME := $(ROOT)/apps/game
SRC := $(GAME)/library/world
TERRAGEN := $(wildcard $(SRC)/terragen/*.cpp)
OBJS := $(patsubst $(SRC)/terragen/%.cpp,$(OBJ)/%.o,$(TERRAGEN))
CXXFLAGS := -std=c++17 -O2 -I$(GAME)/src -I$(GAME)/src/engine -I$(GAME)/library -I$(GAME)/library/world -I$(GAME)/library/space -I$(GAME)/library/matter -I$(GAME)/library/life -I$(GAME)/library/geometry -I$(GAME)/library/vendors/irr/include -I$(GAME)/library/vendors/jsoncpp -MMD -MP
# the tool alone may add more (include paths: the oracle's fastscapelib)
EXTRA ?=

$(OUT): $(OBJS) $(TOOL)
	$(CXX) $(CXXFLAGS) $(EXTRA) $(TOOL) $(OBJS) -o $@ -lpthread

$(OBJ)/%.o: $(SRC)/terragen/%.cpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)
