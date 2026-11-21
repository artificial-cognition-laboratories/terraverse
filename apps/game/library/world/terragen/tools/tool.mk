# terragen built once for the standalone tools (apps/game/library/world/terragen/tools/tool.sh):
# objects kept in OBJ, rebuilt only as their sources or headers change
GAME := $(ROOT)/apps/game
SRC := $(GAME)/library/world/terragen/src
TERRAGEN := $(wildcard $(SRC)/*.cpp $(SRC)/*/*.cpp $(SRC)/*/*/*.cpp)
OBJS := $(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(TERRAGEN))
CXXFLAGS := -std=c++17 -O2 -I$(GAME) -I$(GAME)/src -I$(GAME)/src/engine -I$(GAME)/library -I$(GAME)/library/world -I$(GAME)/library/space -I$(GAME)/library/matter -I$(GAME)/library/life -I$(GAME)/library/geometry -I$(GAME)/library/vendors/irr/include -I$(GAME)/library/vendors/jsoncpp -MMD -MP
# the tool alone may add more (include paths: the oracle's fastscapelib)
EXTRA ?=

$(OUT): $(OBJS) $(TOOL)
	$(CXX) $(CXXFLAGS) $(EXTRA) $(TOOL) $(OBJS) -o $@ -lpthread

$(OBJ)/%.o: $(SRC)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)
