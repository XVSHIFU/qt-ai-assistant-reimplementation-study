PLATFORM_SRC_DIR = $$clean_path($$PWD)

QT += core gui widgets quick network
INCLUDEPATH += $$PLATFORM_SRC_DIR

win32:LIBS += -ladvapi32

HEADERS += \
    $$PLATFORM_SRC_DIR/autostartservice.h \
    $$PLATFORM_SRC_DIR/hotkeyservice.h \
    $$PLATFORM_SRC_DIR/rapooaikeyadapter.h \
    $$PLATFORM_SRC_DIR/singleinstanceservice.h \
    $$PLATFORM_SRC_DIR/systemtraycontroller.h \
    $$PLATFORM_SRC_DIR/windowcontroller.h

SOURCES += \
    $$PLATFORM_SRC_DIR/autostartservice.cpp \
    $$PLATFORM_SRC_DIR/hotkeyservice.cpp \
    $$PLATFORM_SRC_DIR/rapooaikeyadapter.cpp \
    $$PLATFORM_SRC_DIR/singleinstanceservice.cpp \
    $$PLATFORM_SRC_DIR/systemtraycontroller.cpp \
    $$PLATFORM_SRC_DIR/windowcontroller.cpp
