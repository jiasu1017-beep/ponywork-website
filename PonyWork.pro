QT       += core gui network charts printsupport axcontainer sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

win32: LIBS += -lpsapi -lshell32 -lole32 -loleaut32 -lshlwapi -luuid -luser32 -lcrypt32

TARGET = PonyWork
TEMPLATE = app

INCLUDEPATH += $$PWD/modules/user

SOURCES += main.cpp \
           mainwindow.cpp \
           modules/core/database.cpp \
           modules/core/aiconfig.cpp \
           modules/core/logger.cpp \
           modules/core/applicationmanager.cpp \
           modules/core/networkmonitor.cpp \
           modules/core/frpcmanager.cpp \
           modules/core/recommendedappscache.cpp \
           modules/user/userapi.cpp \
           modules/user/userlogindialog.cpp \
           modules/user/usermenuwidget.cpp \
           modules/user/changepassworddialog.cpp \
           modules/widgets/appmanagerwidget.cpp \
           modules/widgets/shutdownwidget.cpp \
           modules/widgets/settingswidget.cpp \
           modules/widgets/collectionmanagerwidget.cpp \
           modules/widgets/remotedesktopwidget.cpp \
           modules/widgets/snapshotmanagerwidget.cpp \
           modules/widgets/appcollectionupdater.cpp \
           modules/widgets/cloud_login_impl.cpp \
           modules/widgets/worklogwidget.cpp \
           modules/widgets/memowidget.cpp \
           modules/widgets/syncconflictdialog.cpp \
           modules/widgets/synclogwidget.cpp \
           modules/widgets/bottomappbar.cpp \
           modules/widgets/userwidget.cpp \
           modules/widgets/recommendappwidget.cpp \
           modules/update/updatemanager.cpp \
           modules/update/updatedialog.cpp \
           modules/update/updateprogressdialog.cpp \
           modules/dialogs/desktopsnapshotdialog.cpp \
           modules/dialogs/batchimportdialog.cpp \
           modules/dialogs/shortcutdialog.cpp \
           modules/dialogs/iconselectordialog.cpp \
           modules/dialogs/aicongeneratordialog.cpp \
           modules/dialogs/chattestdialog.cpp \
           modules/dialogs/aisettingsdialog.cpp

HEADERS  += mainwindow.h \
            modules/core/database.h \
            modules/core/aiconfig.h \
            modules/core/logger.h \
            modules/core/applicationmanager.h \
            modules/core/networkmonitor.h \
            modules/core/frpcmanager.h \
            modules/core/recommendedappscache.h \
            modules/user/userapi.h \
            modules/user/userlogindialog.h \
            modules/user/usermenuwidget.h \
            modules/user/changepassworddialog.h \
            modules/widgets/appmanagerwidget.h \
            modules/widgets/shutdownwidget.h \
            modules/widgets/settingswidget.h \
            modules/widgets/collectionmanagerwidget.h \
            modules/widgets/remotedesktopwidget.h \
            modules/widgets/snapshotmanagerwidget.h \
            modules/widgets/appcollectionupdater.h \
            modules/widgets/worklogwidget.h \
           modules/widgets/memowidget.h \
            modules/widgets/syncconflictdialog.h \
            modules/widgets/synclogwidget.h \
            modules/widgets/bottomappbar.h \
            modules/widgets/userwidget.h \
           modules/widgets/recommendappwidget.h \
            modules/core/appcollectiontypes.h \
            modules/update/updatemanager.h \
            modules/update/updatedialog.h \
            modules/update/updateprogressdialog.h \
            modules/dialogs/desktopsnapshotdialog.h \
            modules/dialogs/batchimportdialog.h \
            modules/dialogs/shortcutdialog.h \
            modules/dialogs/iconselectordialog.h \
            modules/dialogs/aicongeneratordialog.h \
            modules/dialogs/chattestdialog.h \
            modules/dialogs/aisettingsdialog.h \
            modules/core/core.h \
            modules/update/update_module.h \
            modules/widgets/widgets_module.h \
            modules/dialogs/dialogs_module.h

FORMS += modules/ui/appmanagerwidget.ui \
         modules/ui/shutdownwidget.ui \
         modules/ui/settingswidget.ui \
         modules/ui/mainwindow.ui

RESOURCES += resources.qrc

RC_ICONS = img/icon.ico
