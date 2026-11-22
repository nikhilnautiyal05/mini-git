# Qt desktop interface for minigit.
# Open this file in Qt Creator, pick a kit (Desktop / MinGW), then Build and Run.
# Works with Qt 5.9 or newer and with Qt 6.

QT      += core gui widgets
TARGET   = minigit-qt
TEMPLATE = app

# my core code is C++11; Qt 6 itself needs C++17
greaterThan(QT_MAJOR_VERSION, 5): CONFIG += c++17
else: CONFIG += c++11

INCLUDEPATH += ../include

HEADERS += MainWindow.h

# The window code + the SAME core sources the command-line program uses
# (everything in ../src except main.cpp, which is the command-line entry point).
SOURCES += \
    main_qt.cpp \
    MainWindow.cpp \
    ../src/Commands.cpp \
    ../src/Repository.cpp \
    ../src/ObjectStore.cpp \
    ../src/Objects.cpp \
    ../src/Refs.cpp \
    ../src/Index.cpp \
    ../src/Undo.cpp \
    ../src/Diff.cpp \
    ../src/Trie.cpp \
    ../src/Zlib.cpp \
    ../src/Sha1.cpp \
    ../src/FileUtil.cpp \
    ../src/Common.cpp \
    ../src/Server.cpp \
    ../src/UiPage.cpp

win32: LIBS += -lws2_32
