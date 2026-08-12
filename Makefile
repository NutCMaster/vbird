SHELL := powershell.exe
.SHELLFLAGS := -NoProfile -Command
CXX := g++
WINDRES := windres
QT_DIR := D:/Qt/6.11.1/mingw_64
QT_DIR ?= $(QT_INSTALL_PREFIX)
QT_INCLUDE := $(QT_DIR)/include
QT_LIBDIR := $(QT_DIR)/lib
QT_LIBS := -lQt6Widgets -lQt6Gui -lQt6Core
QT_SYS_LIBS := -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -lwinmm -limm32
QT_FLAGS :=
QTLINK :=
ifeq ($(strip $(QT_DIR)),)
    $(warning QT_DIR is not set; Qt headers/libs will not be included automatically.)
else
    QT_FLAGS := -I"$(QT_INCLUDE)" -I"$(QT_INCLUDE)/QtCore" -I"$(QT_INCLUDE)/QtGui" -I"$(QT_INCLUDE)/QtWidgets"
    QTLINK := -L"$(QT_LIBDIR)" $(QT_LIBS) $(QT_SYS_LIBS)
endif
CXXFLAGS := -std=c++23 -Wall -Wextra -O2 $(QT_FLAGS)
SRC_DIR := src
RES_DIR := res
BUILD_DIR := bin
TARGET := $(BUILD_DIR)/vbird.exe
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
RES_SRC := $(RES_DIR)/app.rc
RES_OBJ := $(BUILD_DIR)/app.res

.PHONY: all clean dirs

all: $(TARGET)

dirs:
	@if (!(Test-Path $(BUILD_DIR))) { New-Item -ItemType Directory -Path $(BUILD_DIR) -Force | Out-Null }

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RES_OBJ): $(RES_SRC) | dirs
	$(WINDRES) $(RES_SRC) -O coff -I$(RES_DIR) -o $@

LDFLAGS := $(QTLINK)

$(TARGET): $(OBJS) $(RES_OBJ)
	$(CXX) $(CXXFLAGS) $(OBJS) $(RES_OBJ) $(LDFLAGS) -o $@

clean:
	@if (Test-Path $(BUILD_DIR)) { Remove-Item -Recurse -Force $(BUILD_DIR) }
	@if (Test-Path $(TARGET)) { Remove-Item -Force $(TARGET) }

run: $(TARGET)
	powershell -NoProfile -ExecutionPolicy Bypass -File run.ps1

full: $(BUILD_DIR)
	make clean
	make
	make run