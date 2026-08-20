QT += core testlib
CONFIG += c++11 console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = privacy_tests

INCLUDEPATH += ../../src ../../src/privacy ../../src/settings

SOURCES += \
    test_privacy_consent.cpp \
    ../../src/privacy/privacy_consent_service.cpp \
    ../../src/settings/provider_profile.cpp \
    ../../src/settings/credential_store.cpp \
    ../../src/settings/provider_settings.cpp

HEADERS += \
    ../../src/privacy/privacy_consent_service.h \
    ../../src/settings/provider_profile.h \
    ../../src/settings/credential_store.h \
    ../../src/settings/provider_settings.h

!contains(CONFIG, privacy_service_only) {
    QT += gui widgets network sql
    INCLUDEPATH += ../../src/ai ../../src/app ../../src/storage
    SOURCES += \
        ../../src/app/chatmodels.cpp \
        ../../src/app/dialogmanager.cpp \
        ../../src/ai/openaicompatibleprovider.cpp \
        ../../src/ai/sseparser.cpp \
        ../../src/ai/tokenbudget.cpp \
        ../../src/ai/urlpolicy.cpp \
        ../../src/ai/winhttptransport.cpp \
        ../../src/storage/chat_storage.cpp
    HEADERS += \
        ../../src/app/chatmodels.h \
        ../../src/app/dialogmanager.h \
        ../../src/ai/aitypes.h \
        ../../src/ai/aihttptransport.h \
        ../../src/ai/openaicompatibleprovider.h \
        ../../src/ai/sseparser.h \
        ../../src/ai/tokenbudget.h \
        ../../src/ai/urlpolicy.h \
        ../../src/ai/winhttptransport.h \
        ../../src/storage/chat_storage.h
    win32:LIBS += -lwinhttp
} else {
    DEFINES += PRIVACY_SERVICE_ONLY
}

win32:LIBS += -lcrypt32
