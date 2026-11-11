CC = cc
PYTHON = python3
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD_DIR = build

.PHONY: check check-language check-menu-text check-language-catalog check-font check-peripherals check-arabic check-menu-assets check-host-tools check-native-save check-firmware-save check-mcu-save check-save-endian check-web-save check-web-save-browser check-web-save-capacity check-web-cover check-web-cover-browser check-web-cover-capacity clean

check: check-language check-menu-text check-menu-binary check-language-catalog check-font check-peripherals check-arabic check-host-tools check-native-save check-firmware-save check-mcu-save check-mcu-diskio check-native-cover check-cover-display check-menu-safety check-gif check-mcu-shrink check-file-transfer check-cdc-command-buffer check-audio check-uart check-config check-fatfs check-save-endian check-web-save check-web-cover check-cdgfix

.PHONY: check-cdgfix
check-cdgfix:
	$(PYTHON) -B -m unittest discover -s . -p 'test_cdgfix*.py' -v

.PHONY: check-audio
check-audio:
	$(PYTHON) -B -m unittest discover -s . -p 'test_audio_*.py' -v

.PHONY: check-uart
check-uart:
	$(PYTHON) -B -m unittest discover -s . -p test_uart_tx.py -v

.PHONY: check-menu-safety
check-menu-safety:
	$(PYTHON) -B -m unittest discover -s . -p test_menu_palette.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_menu_timer.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_menu_repeat_transport.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_menu_cdplayer.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_menu_rom.py -v

.PHONY: check-gif
check-gif:
	$(PYTHON) -B -m unittest discover -s . -p 'test_gif_*.py' -v

.PHONY: check-mcu-shrink
check-mcu-shrink:
	$(PYTHON) -B -m unittest discover -s . -p test_rtx_profile.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_image_suffix.py -v

.PHONY: check-file-transfer
check-file-transfer:
	$(PYTHON) -B -m unittest discover -s . -p test_file_transfer.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_menu_word_access.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_menu_file_transfer.py -v

.PHONY: check-cdc-command-buffer
check-cdc-command-buffer:
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_subheader.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_copy.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_filter.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_metadata.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_connections.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_routing.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_put.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_bounds.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_read_ownership.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_sector_input.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_directory.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_xa.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cdc_drive.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cue_geometry.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cue_layout.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_cue_subcode.py -v

.PHONY: check-config
check-config:
	$(PYTHON) -B -m unittest discover -s . -p 'test_config*.py' -v

.PHONY: check-native-cover
check-native-cover:
	$(PYTHON) -B -m unittest discover -s . -p 'test_covertool*.py' -v

.PHONY: check-cover-display
check-cover-display:
	$(PYTHON) -B -m unittest discover -s . -p test_cover_display.py -v

.PHONY: check-mcu-diskio
check-mcu-diskio:
	$(PYTHON) -B -m unittest discover -s . -p test_mcu_diskio.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_sd_safety.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_sd_transfer.py -v
	$(PYTHON) -B -m unittest discover -s . -p test_sd_media.py -v

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
