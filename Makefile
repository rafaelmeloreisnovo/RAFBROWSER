CLANG ?= clang
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/browserraf

.PHONY: all license-check test test-host test-abi test-negative test-pixel test-qemu test-rafascan test-browser-paths clean
all: license-check $(TARGET)

# --license reads this exact Markdown entrypoint at runtime.
license-check:
	test -s LICENSE.md
	grep -q 'GNU General Public License' LICENSE.md

$(TARGET): internal/br_start.S internal/br_main.c $(wildcard internal/*.h)
	mkdir -p $(BUILD_DIR)
	$(CLANG) -O2 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static \
	  -Wl,-e,_start -Wl,-z,noexecstack -Iinternal \
	  internal/br_start.S internal/br_main.c -o $(TARGET)

test: test-host test-abi test-negative test-pixel test-qemu test-rafascan test-browser-paths
test-host:
	sh tests/check_host_freestanding.sh
test-abi:
	sh tests/check_cross_abi_objects.sh
test-negative:
	sh tests/check_negative_fixtures.sh
test-pixel:
	sh tests/check_pixel_matrix.sh
test-qemu:
	sh tests/check_qemu_abis.sh
test-rafascan:
	sh tests/test_rafascan_network.sh
test-browser-paths:
	mkdir -p $(BUILD_DIR)
	$(CLANG) -std=c11 -Wall -Wextra -Werror -I. tests/test_browser_paths.c -o $(BUILD_DIR)/test_browser_paths
	$(BUILD_DIR)/test_browser_paths
clean:
	rm -rf $(BUILD_DIR)
