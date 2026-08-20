QT += core gui widgets quick qml network testlib
CONFIG += c++11 console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = platform_tests

win32:LIBS += -ladvapi32

PLATFORM_DIR = $$clean_path($$PWD/../../src/platform)
APP_DIR = $$clean_path($$PWD/../../src/app)
INCLUDEPATH += $$PLATFORM_DIR $$APP_DIR

SOURCES += \
    platform_services_test.cpp \
    $$APP_DIR/lifecyclecoordinator.cpp \
    $$PLATFORM_DIR/autostartservice.cpp \
    $$PLATFORM_DIR/hotkeyservice.cpp \
    $$PLATFORM_DIR/rapooaikeyadapter.cpp \
    $$PLATFORM_DIR/singleinstanceservice.cpp \
    $$PLATFORM_DIR/systemtraycontroller.cpp \
    $$PLATFORM_DIR/windowcontroller.cpp

HEADERS += \
    $$APP_DIR/lifecyclecoordinator.h \
    $$PLATFORM_DIR/autostartservice.h \
    $$PLATFORM_DIR/hotkeyservice.h \
    $$PLATFORM_DIR/rapooaikeyadapter.h \
    $$PLATFORM_DIR/singleinstanceservice.h \
    $$PLATFORM_DIR/systemtraycontroller.h \
    $$PLATFORM_DIR/windowcontroller.h
