QT += quick 3dcore 3drender 3dquick 3danimation 3dquickextras concurrent

SOURCES += \
    src/main.cpp \
    src/sceneplayer.cpp \
    src/twospaceparser.cpp

HEADERS += \
    src/sceneplayer.h \
    src/twospaceparser.h

OTHER_FILES += \
    src/main.qml

RESOURCES += \
    src/rtscplq3t.qrc
