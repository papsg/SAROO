CC = cc
PYTHON = python3
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD_DIR = build

.PHONY: check check-language check-menu-text check-language-catalog check-font check-peripherals check-arabic check-menu-assets clean

check: check-language check-menu-text check-language-catalog check-font check-peripherals check-arabic check-menu-assets

check-arabic:
	$(PYTHON) -B -m unittest discover -s . -p test_arabic.py -v

check-menu-assets:
	cargo fmt --manifest-path ../tools/Cargo.toml --check
	cargo clippy --locked --manifest-path ../tools/Cargo.toml --all-targets -- -D warnings
	cargo test --locked --manifest-path ../tools/Cargo.toml

check-peripherals:
	$(PYTHON) -B -m unittest discover -s . -p test_menu_peripherals.py -v

check-font:
	$(PYTHON) -B -m unittest discover -s . -p test_font_coverage.py -v

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
