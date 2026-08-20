QT += core
QT -= gui
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = write_deepseek_profile

INCLUDEPATH += ../../src

SOURCES += \
    install_deepseek_profile.cpp \
    ../../src/settings/provider_profile.cpp \
    ../../src/settings/provider_settings.cpp \
    ../../src/settings/credential_store.cpp

HEADERS += \
    ../../src/settings/provider_profile.h \
    ../../src/settings/provider_settings.h \
    ../../src/settings/credential_store.h

win32:LIBS += -lcrypt32
