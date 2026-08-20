QT += quick qml gui widgets network sql
CONFIG += c++11
win32:CONFIG(release, debug|release) {
    CONFIG -= console
    CONFIG += windows
}
CONFIG += resources_big
CONFIG -= app_bundle
TEMPLATE = app
TARGET = SmartKeyAIReconstruction
include(version.pri)

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/main.cpp \
    src/app/chatmodels.cpp \
    src/app/dialogmanager.cpp \
    src/app/hotkeyfacade.cpp \
    src/app/lifecyclecoordinator.cpp \
    src/app/settingfacade.cpp \
    src/app/uipreferences.cpp \
    src/app/windowfacade.cpp \
    src/ai/legacyrapooprovider.cpp \
    src/ai/modeldiscoveryservice.cpp \
    src/ai/openaicompatibleprovider.cpp \
    src/ai/sseparser.cpp \
    src/ai/tokenbudget.cpp \
    src/ai/urlpolicy.cpp \
    src/ai/winhttptransport.cpp \
    src/diagnostics/jsonl_logger.cpp \
    src/diagnostics/diagnostics_exporter.cpp \
    src/data/data_management_service.cpp \
    src/privacy/privacy_consent_service.cpp \
    src/mock/models.cpp \
    src/mock/backendmocks.cpp \
    src/settings/credential_store.cpp \
    src/settings/provider_profile.cpp \
    src/settings/provider_settings.cpp \
    src/storage/chat_storage.cpp \
    src/storage/history_exporter.cpp

HEADERS += \
    src/app/chatmodels.h \
    src/app/dialogmanager.h \
    src/app/hotkeyfacade.h \
    src/app/lifecyclecoordinator.h \
    src/app/managerfacade.h \
    src/app/settingfacade.h \
    src/app/startupdecision.h \
    src/app/uipreferences.h \
    src/app/windowfacade.h \
    src/ai/aihttptransport.h \
    src/ai/aitypes.h \
    src/ai/legacyrapooprovider.h \
    src/ai/modeldiscoveryservice.h \
    src/ai/openaicompatibleprovider.h \
    src/ai/sseparser.h \
    src/ai/tokenbudget.h \
    src/ai/urlpolicy.h \
    src/ai/winhttptransport.h \
    src/diagnostics/jsonl_logger.h \
    src/diagnostics/diagnostics_exporter.h \
    src/data/data_management_service.h \
    src/privacy/privacy_consent_service.h \
    src/mock/models.h \
    src/mock/backendmocks.h \
    src/settings/credential_store.h \
    src/settings/provider_profile.h \
    src/settings/provider_settings.h \
    src/storage/chat_storage.h \
    src/storage/history_exporter.h

include(src/platform/platform.pri)

win32:LIBS += -lwinhttp -lcrypt32 -luser32

RESOURCES += resources.qrc

TRANSLATIONS += \
    translations/smartkey_zh_CN.ts \
    translations/smartkey_en_US.ts
CONFIG += lrelease embed_translations
QM_FILES_RESOURCE_PREFIX = /i18n

DEFINES += QT_DEPRECATED_WARNINGS
