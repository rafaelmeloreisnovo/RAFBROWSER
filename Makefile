CLANG ?= clang
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/browserraf

.PHONY: all test test-host test-abi test-negative test-pixel test-qemu test-rafascan clean
all: $(TARGET)

$(TARGET): internal/br_start.S internal/br_main.c $(wildcard internal/*.h)
	mkdir -p $(BUILD_DIR)
	$(CLANG) -O2 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static \
	  -Wl,-e,_start -Wl,-z,noexecstack -Iinternal \
	  internal/br_start.S internal/br_main.c -o $(TARGET)

test: test-host test-abi test-negative test-pixel test-qemu test-rafascan
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
clean:
	rm -rf $(BUILD_DIR)
