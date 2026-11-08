CC = cc
PYTHON = python3
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD_DIR = build

.PHONY: check check-language check-menu-text check-language-catalog clean

check: check-language check-menu-text check-language-catalog

check-language-catalog:
	$(PYTHON) -B -m unittest discover -s . -p test_language_catalog.py -v

check-menu-text:
	$(PYTHON) -B -m unittest discover -s . -p test_menu_text.py -v

check-language: $(BUILD_DIR)/language
	UBSAN_OPTIONS=halt_on_error=1 $(BUILD_DIR)/language

$(BUILD_DIR)/language: language.c ../Firm_Saturn/language.c ../Firm_Saturn/main.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) language.c -o $@

clean:
	rm -rf $(BUILD_DIR)
