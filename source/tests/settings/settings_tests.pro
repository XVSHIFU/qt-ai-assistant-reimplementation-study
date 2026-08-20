QT += core testlib
QT -= gui
CONFIG += console testcase c++11
DEFINES += SMARTKEY_SETTINGS_TESTS
TEMPLATE = app
TARGET = settings_tests

INCLUDEPATH += ../../src ../../src/settings ../../src/diagnostics ../../src/privacy

SOURCES += \
    test_provider_settings.cpp \
    ../../src/settings/provider_profile.cpp \
    ../../src/settings/credential_store.cpp \
    ../../src/settings/provider_settings.cpp \
    ../../src/privacy/privacy_consent_service.cpp \
    ../../src/diagnostics/jsonl_logger.cpp

HEADERS += \
    ../../src/settings/provider_profile.h \
    ../../src/settings/credential_store.h \
    ../../src/settings/provider_settings.h \
    ../../src/privacy/privacy_consent_service.h \
    ../../src/diagnostics/jsonl_logger.h

win32:LIBS += -lcrypt32
