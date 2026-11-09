CC = cc
PYTHON = python3
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD_DIR = build

.PHONY: check check-language check-menu-text check-language-catalog check-font check-peripherals check-arabic check-menu-assets check-host-tools check-native-save check-firmware-save check-mcu-save check-save-endian check-web-save check-web-save-browser check-web-save-capacity check-web-cover check-web-cover-browser check-web-cover-capacity clean

check: check-language check-menu-text check-menu-binary check-language-catalog check-font check-peripherals check-arabic check-host-tools check-native-save check-firmware-save check-mcu-save check-mcu-diskio check-cover-display check-fatfs check-save-endian check-web-save check-web-cover

.PHONY: check-cover-display
check-cover-display:
	$(PYTHON) -B -m unittest discover -s . -p test_cover_display.py -v

.PHONY: check-mcu-diskio
check-mcu-diskio:
	$(PYTHON) -B -m unittest discover -s . -p test_mcu_diskio.py -v

.PHONY: check-fatfs
check-fatfs:
	$(PYTHON) -B -m unittest discover -s . -p test_fatfs_seek.py -v

.PHONY: check-menu-binary
check-menu-binary:
	$(PYTHON) -B -m unittest discover -s . -p test_menu_binary_bounds.py -v

check-firmware-save:
	$(PYTHON) -B -m unittest discover -s . -p 'test_firmware_save*.py' -v

check-mcu-save:
	$(PYTHON) -B -m unittest discover -s . -p 'test_mcu_save*.py' -v

check-native-save:
	$(PYTHON) -B -m unittest discover -s . -p 'test_mems_*.py' -v
	$(PYTHON) -B -m unittest discover -s . -p 'test_savetool_*.py' -v

check-web-cover:
	$(PYTHON) -B -m unittest discover -s . -p test_web_cover.py -v

check-web-cover-browser:
	$(PYTHON) -B web_cover_fixtures.py build/web-cover-browser
	node web_cover_browser.cjs
	node web_cover_picker.cjs ../tools/covertool/web/index.html build/web-cover-browser build/web-cover-browser/picker-results.json
	node web_cover_render.cjs ../tools/covertool/web/index.html build/web-cover-render

check-web-cover-capacity:
	$(PYTHON) -B web_cover_fixtures.py --capacity build/web-cover-capacity
	node web_cover_capacity.cjs build/web-cover-capacity/full-unique.bin build/web-cover-app-capacity ../tools/covertool/web/index.html

check-save-endian:
	$(PYTHON) -B -m unittest discover -s . -p test_save_endian.py -v

check-web-save:
	$(PYTHON) -B -m unittest discover -s . -p 'test_web_save*.py' -v

check-web-save-browser:
	$(PYTHON) -B web_save_ui.py build/web-save-ui-tests
	node web_save_browser.cjs build/web-save-ui-tests ../tools/savetool/web/index.html
	node web_save_render.cjs ../tools/savetool/web/index.html build/web-save-ui-tests build/web-save-render

check-web-save-capacity:
	$(PYTHON) -B web_save_ui.py --capacity build/web-save-capacity
	node web_save_app_capacity.cjs build/web-save-capacity/full-saroo.bin build/web-save-app-capacity ../tools/savetool/web/index.html

check-arabic:
	$(PYTHON) -B -m unittest discover -s . -p test_arabic.py -v

check-menu-assets: check-host-tools

check-host-tools:
	cargo fmt --manifest-path ../tools/Cargo.toml --all --check
	cargo clippy --locked --manifest-path ../tools/Cargo.toml --workspace --all-targets -- -D warnings
	cargo test --locked --manifest-path ../tools/Cargo.toml --workspace

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
