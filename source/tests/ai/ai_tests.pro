QT += core network testlib
CONFIG += c++11 console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_ai

INCLUDEPATH += ../../src/ai

SOURCES += \
    tst_ai.cpp \
    ../../src/ai/urlpolicy.cpp \
    ../../src/ai/sseparser.cpp \
    ../../src/ai/tokenbudget.cpp \
    ../../src/ai/winhttptransport.cpp \
    ../../src/ai/openaicompatibleprovider.cpp \
    ../../src/ai/legacyrapooprovider.cpp \
    ../../src/ai/modeldiscoveryservice.cpp

HEADERS += \
    ../../src/ai/aitypes.h \
    ../../src/ai/aihttptransport.h \
    ../../src/ai/urlpolicy.h \
    ../../src/ai/sseparser.h \
    ../../src/ai/tokenbudget.h \
    ../../src/ai/winhttptransport.h \
    ../../src/ai/openaicompatibleprovider.h \
    ../../src/ai/legacyrapooprovider.h \
    ../../src/ai/modeldiscoveryservice.h

win32:LIBS += -lwinhttp

DISTFILES += \
    fixtures/openai_stream.sse \
    fixtures/error_429.json
