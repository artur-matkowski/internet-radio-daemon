CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2
LDFLAGS  := -lmosquitto

SRCDIR   := src
BUILDDIR := build
TARGET   := $(BUILDDIR)/rpiradio

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

PREFIX   := /usr/local
BINDIR   := $(PREFIX)/bin
CONFDIR  := /etc/rpiradio
UNITDIR  := /etc/systemd/system

.PHONY: all clean install-deps install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

install-deps:
	apt-get update
	apt-get install -y g++ pkg-config mpv libmosquitto-dev libevdev-dev nlohmann-json3-dev

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/rpiradio
	install -D -m 644 config/default_config.json $(DESTDIR)$(CONFDIR)/config.json
	install -D -m 644 systemd/rpiradio.service $(DESTDIR)$(UNITDIR)/rpiradio.service
	id -u rpiradio >/dev/null 2>&1 || useradd -r -s /usr/sbin/nologin rpiradio
	usermod -aG audio rpiradio
	systemctl daemon-reload
	systemctl enable rpiradio.service

uninstall:
	systemctl disable --now rpiradio.service || true
	rm -f $(DESTDIR)$(BINDIR)/rpiradio
	rm -f $(DESTDIR)$(UNITDIR)/rpiradio.service
	systemctl daemon-reload

-include $(DEPS)
