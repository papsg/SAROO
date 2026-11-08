CC = cc
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror
SANITIZERS = -fsanitize=address,undefined -fno-omit-frame-pointer
BUILD_DIR = build

.PHONY: check check-language clean

check: check-language

check-language: $(BUILD_DIR)/language
	UBSAN_OPTIONS=halt_on_error=1 $(BUILD_DIR)/language

$(BUILD_DIR)/language: language.c ../Firm_Saturn/language.c ../Firm_Saturn/main.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SANITIZERS) language.c -o $@

clean:
	rm -rf $(BUILD_DIR)
