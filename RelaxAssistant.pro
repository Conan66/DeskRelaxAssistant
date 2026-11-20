# ============================================================
# 桌面放松助手 —— Qt 工程文件
# ------------------------------------------------------------
# 用 Qt 5.14.2 + MSVC 编译（VS2022 可直接打开本 .pro）
# 依赖模块：core gui widgets（界面）
#           multimedia（播语音和节拍音）
# ============================================================

QT       += core gui widgets multimedia

CONFIG   += c++11

# 源码是 UTF-8 编码，中文版 Windows 的 MSVC 默认按 GBK 读会乱码，
# 这里显式告诉编译器按 UTF-8 解析（否则中文注释会让编译直接报错）
msvc: QMAKE_CXXFLAGS += /utf-8

TARGET   = RelaxAssistant
TEMPLATE = app

# 资源文件：示意图 + 全部语音 + 节拍音，全部打进 exe，运行不依赖外部文件夹
RESOURCES += resources.qrc

# 久坐提醒要用 OpenInputDesktop 判断是否锁屏，它在 user32 里
LIBS += -luser32

# ------------------------------------------------------------
# 源码（按 core / ui 分目录，方便以后扩展）
# ------------------------------------------------------------
INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/core/BreathTypes.cpp \
    src/core/BreathingEngine.cpp \
    src/core/AppSettings.cpp \
    src/core/AudioManager.cpp \
    src/core/MoodLib.cpp \
    src/core/NoiseGen.cpp \
    src/core/SitReminder.cpp \
    src/core/StretchLib.cpp \
    src/ui/AmbientView.cpp \
    src/ui/BreathingBall.cpp \
    src/ui/BodyPoseView.cpp \
    src/ui/DiagramView.cpp \
    src/ui/DesktopBallWindow.cpp \
    src/ui/GuideWindow.cpp \
    src/ui/MoodDialog.cpp \
    src/ui/PlanDialog.cpp \
    src/ui/RecordsDialog.cpp \
    src/ui/RhythmDialog.cpp \
    src/ui/ReminderOverlay.cpp \
    src/ui/ScreenDpiGuard.cpp \
    src/ui/SitReminderDialog.cpp \
    src/ui/StretchWindow.cpp

HEADERS += \
    src/core/BreathTypes.h \
    src/core/BreathingEngine.h \
    src/core/AppSettings.h \
    src/core/AudioManager.h \
    src/core/MoodLib.h \
    src/core/NoiseGen.h \
    src/core/SitReminder.h \
    src/core/StretchLib.h \
    src/ui/AmbientView.h \
    src/ui/BreathingBall.h \
    src/ui/BodyPoseView.h \
    src/ui/DiagramView.h \
    src/ui/DesktopBallWindow.h \
    src/ui/GuideWindow.h \
    src/ui/MoodDialog.h \
    src/ui/PlanDialog.h \
    src/ui/RecordsDialog.h \
    src/ui/RhythmDialog.h \
    src/ui/ReminderOverlay.h \
    src/ui/ScreenDpiGuard.h \
    src/ui/SitReminderDialog.h \
    src/ui/StretchWindow.h
