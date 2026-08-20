QT += core
QT -= gui
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = live_deepseek_smoke

INCLUDEPATH += ../../src

SOURCES += \
    live_deepseek_smoke.cpp \
    ../../src/ai/winhttptransport.cpp \
    ../../src/ai/urlpolicy.cpp \
    ../../src/ai/sseparser.cpp \
    ../../src/ai/openaicompatibleprovider.cpp \
    ../../src/ai/modeldiscoveryservice.cpp \
    ../../src/settings/credential_store.cpp \
    ../../src/settings/provider_profile.cpp \
    ../../src/settings/provider_settings.cpp

HEADERS += \
    ../../src/ai/aitypes.h \
    ../../src/ai/aihttptransport.h \
    ../../src/ai/winhttptransport.h \
    ../../src/ai/urlpolicy.h \
    ../../src/ai/sseparser.h \
    ../../src/ai/openaicompatibleprovider.h \
    ../../src/ai/modeldiscoveryservice.h \
    ../../src/settings/credential_store.h \
    ../../src/settings/provider_profile.h \
    ../../src/settings/provider_settings.h

win32:LIBS += -lwinhttp -lcrypt32
