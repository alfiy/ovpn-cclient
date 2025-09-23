# Makefile for OVPN Client (C + GTK3 + AppIndicator)

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
PKGCONFIG = pkg-config

# 包依赖 - 使用检测到的AppIndicator包
PACKAGES = gtk+-3.0 libnm ayatana-appindicator3-0.1 uuid

# 编译标志
CFLAGS += $(shell $(PKGCONFIG) --cflags $(PACKAGES))
LIBS = $(shell $(PKGCONFIG) --libs $(PACKAGES))

# 目标文件
TARGET = ovpn-client
SOURCE = ovpn_client.c

# 默认目标
all: $(TARGET)

# 编译主程序
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# 安装
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)
	mkdir -p ~/.local/share/applications
	cp ovpn-client.desktop ~/.local/share/applications/
	update-desktop-database ~/.local/share/applications/ || true

# 清理
clean:
	rm -f $(TARGET)

# 检查依赖
check-deps:
	@echo "Checking dependencies..."
	@$(PKGCONFIG) --exists $(PACKAGES) && echo "All dependencies found" || echo "Missing dependencies"
	@echo "Required packages:"
	@echo "  - libgtk-3-dev"
	@echo "  - libnm-dev" 
	@echo "  - ayatana-appindicator3-0.1 development package"
	@echo "  - uuid-dev"

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: all install clean check-deps debug
