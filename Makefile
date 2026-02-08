CC ?= cc

INCLUDES ?= -Isrc

CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -pedantic -Werror -std=c23 -g \
					-fsanitize=address -fsanitize=undefined

SRC_DIR    = src
BUILD_DIR ?= build
OBJ_DIR    = $(BUILD_DIR)/obj

PREFIX ?= /usr/local
BINDIR  = $(PREFIX)/bin
TARGET  = zc

#
# Build rules
#

all: $(BUILD_DIR)/$(TARGET)

APPLICATION_SOURCES  = $(wildcard $(SRC_DIR)/*.c)
APPLICATION_SOURCES += $(wildcard $(SRC_DIR)/*/*.c)
APPLICATION_SOURCES += $(wildcard $(SRC_DIR)/*/*/*.c)

define src_to_obj
$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(1))
endef

OBJECTS = $(foreach src,$(APPLICATION_SOURCES),$(call src_to_obj,$(src)))

$(BUILD_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDES) $(CFLAGS) $(OBJECTS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ -c $^

clean:
	rm -rf $(BUILD_DIR)

#
# Installing
#

install: $(BUILD_DIR)/$(TARGET)
	@echo "Installing to $(BINDIR)..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BUILD_DIR)/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	@echo "Removing from $(BINDIR)..."
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: all clean install uninstall
