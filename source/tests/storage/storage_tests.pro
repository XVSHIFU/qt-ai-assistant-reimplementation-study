QT += core gui network testlib sql
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = storage_tests

INCLUDEPATH += ../../src ../../src/storage

SOURCES += \
    test_chat_storage.cpp \
    ../../src/app/chatmodels.cpp \
    ../../src/app/dialogmanager.cpp \
    ../../src/ai/openaicompatibleprovider.cpp \
    ../../src/ai/sseparser.cpp \
    ../../src/ai/tokenbudget.cpp \
    ../../src/ai/urlpolicy.cpp \
    ../../src/ai/winhttptransport.cpp \
    ../../src/privacy/privacy_consent_service.cpp \
    ../../src/data/data_management_service.cpp \
    ../../src/diagnostics/diagnostics_exporter.cpp \
    ../../src/settings/credential_store.cpp \
    ../../src/settings/provider_profile.cpp \
    ../../src/settings/provider_settings.cpp \
    ../../src/storage/chat_storage.cpp \
    ../../src/storage/history_exporter.cpp

HEADERS += \
    ../../src/app/chatmodels.h \
    ../../src/app/dialogmanager.h \
    ../../src/ai/aihttptransport.h \
    ../../src/ai/openaicompatibleprovider.h \
    ../../src/ai/tokenbudget.h \
    ../../src/ai/winhttptransport.h \
    ../../src/privacy/privacy_consent_service.h \
    ../../src/data/data_management_service.h \
    ../../src/diagnostics/diagnostics_exporter.h \
    ../../src/settings/provider_settings.h \
    ../../src/storage/chat_storage.h \
    ../../src/storage/history_exporter.h

win32:LIBS += -lwinhttp -lcrypt32
