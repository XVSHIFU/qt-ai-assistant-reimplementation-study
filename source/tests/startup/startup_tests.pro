QT += core testlib
CONFIG += c++11 console testcase
TEMPLATE = app
TARGET = startup_tests

INCLUDEPATH += ../../src
SOURCES += startup_decision_test.cpp
HEADERS += ../../src/app/startupdecision.h
