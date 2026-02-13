CXX := g++
CXXFLAGS := -O3 -march=native -funroll-loops -mtune=native --std=c++2a
CXXFLAGS += -DUSE_SIM_DDR -DUSE_SIM_DDR_AXI4 -DICACHE_MISS_LATENCY=8

INCLUDES := -I./include \
            -I./src/ref/include \
            -I./src/simddr/include \
            -I./src/axi/include

LDFLAGS := -lz -lstdc++fs
LIBS := ./third_party/softfloat/softfloat.a

CORE_SRCS := src/sc_axi4_sim_api.cpp \
             src/ref/ref_standalone.cpp \
             src/axi/AXI_Interconnect.cpp

EXE_SRCS := src/main.cpp \
            src/simddr/SimDDR.cpp

CORE_OBJS := $(CORE_SRCS:.cpp=.o)
EXE_OBJS := $(EXE_SRCS:.cpp=.o)

TARGET := single_cycle_axi4.out
STATIC_LIB := libsingle_cycle_axi4.a
SHARED_LIB := libsingle_cycle_axi4.so

.PHONY: all clean lib-static lib-shared libs run-dhrystone run-coremark run-linux

all: $(TARGET)

libs: $(STATIC_LIB) $(SHARED_LIB)

lib-static: $(STATIC_LIB)

lib-shared: $(SHARED_LIB)

$(STATIC_LIB): $(CORE_OBJS)
	ar rcs $@ $^

$(SHARED_LIB): $(CORE_OBJS)
	$(CXX) -shared -Wl,--allow-shlib-undefined -o $@ $^ $(LDFLAGS)

$(TARGET): $(EXE_OBJS) $(STATIC_LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $(LIBS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -fPIC $(INCLUDES) -c $< -o $@

run-dhrystone: $(TARGET)
	./$(TARGET) bin/dhrystone.bin

run-coremark: $(TARGET)
	./$(TARGET) bin/coremark.bin

run-linux: $(TARGET)
	./$(TARGET) bin/linux.bin

clean:
	rm -f $(TARGET) $(STATIC_LIB) $(SHARED_LIB) $(CORE_OBJS) $(EXE_OBJS)
