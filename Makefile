BUILD_TYPE?=Release
BUILD_DIR?=build
PKG_NAME?=yanf

default: configure make

debug:
	$(eval BUILD_TYPE := Debug)

cmake_check:
	@cmake --version || (echo "\n** Please install 'cmake' **\n" && exit 1)

configure: cmake_check
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ../src
	@echo "Modify build configuration in '$(BUILD_DIR)'"

make:
	cd $(BUILD_DIR) && make

install:
	cd $(BUILD_DIR) && make install

clean:
	rm -r $(BUILD_DIR) || true
	rm tags || true

all: clean default

pkg:
	cd $(BUILD_DIR) && make package

pkg_install:
	sudo dpkg -i $(BUILD_DIR)/$(PKG_NAME)*.deb

pkg_uninstall:
	sudo apt-get --yes remove $(PKG_NAME) || true

tags:
	ctags * -R
	cscope -b `find . -name '*.c'` `find . -name '*.h'`

umount:
	@sudo umount /tmp/yanf || true

mount: umount
	@sudo rm -r /tmp/yanf || true
	@sudo mkdir -p /tmp/yanf || true
	@sudo yanf -f -o direct_io,big_writes,max_write=131072,max_read=131072 /tmp/yanf &

yanf:
	@make mount
	@sudo ls -lah /tmp/yanf || true
	@sudo sh -c "touch /tmp/yanf/fnay" || true
	@make umount

dev: pkg_uninstall all pkg pkg_install
