CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 $(shell pkg-config --cflags libevdev)
LDFLAGS  := $(shell pkg-config --libs libevdev) -lmosquitto

SRCDIR   := src
BUILDDIR := build
TARGET   := $(BUILDDIR)/rpiradio

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

-include $(DEPS)
