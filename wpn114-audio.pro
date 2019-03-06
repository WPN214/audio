TARGET = WPN114-audio
TEMPLATE = lib
CONFIG += c++14 dll
QT += quick

localmod: DESTDIR = $$QML_MODULE_DESTDIR/WPN114/Audio
else {
    DESTDIR = $$[QT_INSTALL_QML]/WPN114/Audio
    QML_MODULE_DESTDIR = $$[QT_INSTALL_QML]
}

QMLDIR_FILES += $$PWD/qml/qmldir
QMLDIR_FILES += $$PWD/qml/audio.qmltypes
OTHER_FILES = $$QMLDIR_FILES

macx: QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/

for(FILE,QMLDIR_FILES) {
    QMAKE_POST_LINK += $$quote(cp $${FILE} $${DESTDIR}$$escape_expand(\n\t))
}

macx {
    LIBS += -framework CoreFoundation
    LIBS += -framework CoreAudio
    DEFINES += __MACOSX_CORE__
}

linux {
    DEFINES += __LINUX_ALSA__
    DEFINES += __UNIX_JACK__
    LIBS += -lpthread -ljack -lasound
}

HEADERS += $$PWD/external/rtaudio/RtAudio.h
HEADERS += $$PWD/source/stream.h
HEADERS += $$PWD/source/graph.h
HEADERS += $$PWD/source/utilities.h
HEADERS += $$PWD/source/qtwrapper.hpp

SOURCES += $$PWD/external/rtaudio/RtAudio.cpp
SOURCES += $$PWD/source/stream.c
SOURCES += $$PWD/source/graph.c
SOURCES += $$PWD/source/utilities.c
SOURCES += $$PWD/source/qtwrapper.cpp

SOURCES += $$PWD/qml_plugin.cpp
HEADERS += $$PWD/qml_plugin.hpp

DEFINES += WPN_DOUBLE_PRECISION

DISTFILES += examples/Audio.qml
