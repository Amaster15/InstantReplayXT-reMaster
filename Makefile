# Linux fallback build using the installed OBS development headers and Qt 6.
CXX ?= g++
PKG_CONFIG ?= pkg-config
MOC ?= $(shell qmake6 -query QT_HOST_LIBEXECS)/moc
OBS_INCLUDE ?= /usr/include/obs
CPPFLAGS += -I. -I$(OBS_INCLUDE) $(shell $(PKG_CONFIG) --cflags Qt6Widgets)
CXXFLAGS += -std=c++17 -fPIC -Wall -Wextra
LDLIBS += $(shell $(PKG_CONFIG) --libs Qt6Widgets) -lobs -lobs-frontend-api
SOURCES = module-entrance.cpp InstantReplay.cpp InstantReplay_callout.cpp
HEADERS = InstantReplay.h InstantReplay_callout.h ui_InstantReplay_callback.h
.PHONY: all test clean
all: build/InstantReplay.so
build:
	mkdir -p $@
build/moc_InstantReplay_callout.cpp: InstantReplay_callout.h | build
	$(MOC) $< -o $@
build/InstantReplay.so: $(SOURCES) $(HEADERS) build/moc_InstantReplay_callout.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -shared $(SOURCES) build/moc_InstantReplay_callout.cpp -o $@ $(LDFLAGS) $(LDLIBS)
build/replay-controller-test: tests/replay-controller-test.cpp InstantReplay.cpp InstantReplay.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)
test: build/replay-controller-test
	./build/replay-controller-test
clean:
	rm -rf build
