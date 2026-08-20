# Single application/release version source. Keep SemVer numeric so qmake can
# generate the four-part Windows VERSIONINFO values deterministically.
SMARTKEY_APP_VERSION = 2.0.0
isEmpty(SMARTKEY_APP_VERSION): error("SMARTKEY_APP_VERSION must be defined")
!contains(SMARTKEY_APP_VERSION, ^[0-9]+\.[0-9]+\.[0-9]+$): error("SMARTKEY_APP_VERSION must be numeric SemVer")

VERSION = $$SMARTKEY_APP_VERSION
DEFINES += SMARTKEY_APP_VERSION=\\\"$$SMARTKEY_APP_VERSION\\\"
QMAKE_TARGET_PRODUCT = SmartKey AI
QMAKE_TARGET_COMPANY = SmartKeyAI
QMAKE_TARGET_DESCRIPTION = SmartKey AI Desktop Assistant
QMAKE_TARGET_COPYRIGHT = Copyright (C) 2026 SmartKeyAI
