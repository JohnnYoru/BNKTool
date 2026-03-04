CXX = g++
MINGW = x86_64-w64-mingw32-g++
CXXFLAGS = -std=c++17 -O2 -Wall

TARGETS_WIN = BNKTool.exe BNK-Unpack.exe BNK-Pack.exe
TARGETS_LINUX = BNKTool BNK-Unpack BNK-Pack

all: linux windows

linux: $(TARGETS_LINUX)

windows: $(TARGETS_WIN)

# Windows builds
BNKTool.exe: bnktool.cpp
	$(MINGW) $< -o $@ $(CXXFLAGS) -static

BNK-Unpack.exe: bnk-unpack.cpp
	$(MINGW) $< -o $@ $(CXXFLAGS) -static

BNK-Pack.exe: bnk-pack.cpp
	$(MINGW) $< -o $@ $(CXXFLAGS) -static

# Linux builds
BNKTool: bnktool.cpp
	$(CXX) $< -o $@ $(CXXFLAGS)

BNK-Unpack: bnk-unpack.cpp
	$(CXX) $< -o $@ $(CXXFLAGS)

BNK-Pack: bnk-pack.cpp
	$(CXX) $< -o $@ $(CXXFLAGS)

clean:
	rm -f $(TARGETS_WIN) $(TARGETS_LINUX)

.PHONY: all linux windows clean
