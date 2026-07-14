#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "ui/mainwindow.hpp"

extern "C" {
#include "luksnc.h"
#include "log.h"
}

#define LOCKFILE "/tmp/recovery-qt.pid"
#define LOGFILE  "/tmp/recovery-qt.log"

static int checkAndCleanupPrevious()
{
    QFile lock(LOCKFILE);
    if (!lock.exists())
        return 0;

    if (!lock.open(QIODevice::ReadOnly))
        return -1;
    QTextStream in(&lock);
    bool ok = false;
    int stored_pid = in.readLine().trimmed().toInt(&ok);
    lock.close();

    if (ok && stored_pid > 1) {
        if (kill(stored_pid, 0) == 0)
            return stored_pid;
        unlink(LOCKFILE);
        luks_cleanup_orphans();
        return 0;
    }
    unlink(LOCKFILE);
    return 0;
}

static bool writeLock()
{
    QFile lock(LOCKFILE);
    if (!lock.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QTextStream out(&lock);
    out << (int)getpid() << "\n";
    lock.close();
    return true;
}

static QStringList collectDisplayEnv()
{
    QStringList env;
    auto add = [&](const char *name) {
        QByteArray val = qgetenv(name);
        if (!val.isEmpty())
            env << QString::fromLatin1(name) + "=" + QString::fromLocal8Bit(val);
    };
    add("DISPLAY");
    add("XAUTHORITY");
    add("WAYLAND_DISPLAY");
    add("XDG_RUNTIME_DIR");
    add("DBUS_SESSION_BUS_ADDRESS");
    add("QT_QPA_PLATFORM");
    return env;
}

int main(int argc, char *argv[])
{
    if (geteuid() != 0) {
        QApplication app(argc, argv);
        app.setApplicationName("recovery-qt");

        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle("Root Privileges Required");
        msgBox.setText("recovery-qt needs root privileges to access disk devices directly.");
        msgBox.setInformativeText("The application will restart with administrative privileges.\n"
                                  "You may be prompted for your password.");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);

        if (msgBox.exec() == QMessageBox::Cancel)
            return 0;

        QString binPath = QDir::cleanPath(QCoreApplication::applicationFilePath());
        QStringList pkexecArgs;
        pkexecArgs << "env";
        pkexecArgs.append(collectDisplayEnv());
        pkexecArgs << ("RECOVERY_QT_CWD=" + QDir::currentPath());
        pkexecArgs << binPath;
        for (int i = 1; i < argc; i++)
            pkexecArgs << QString::fromLocal8Bit(argv[i]);

        if (!QProcess::startDetached("pkexec", pkexecArgs)) {
            QMessageBox::critical(nullptr, "Error",
                "Failed to launch pkexec. Please install policykit-1 or run with sudo.");
            return 1;
        }
        return 0;
    }

    QApplication app(argc, argv);
    app.setApplicationName("recovery-qt");
    app.setApplicationVersion("7.3");
    app.setOrganizationName("recovery-qt");

    {
        QFile test(LOGFILE);
        if (test.open(QIODevice::WriteOnly)) {
            test.write("recovery-qt log\n", 17);
            test.close();
        }
        int log_errno;
        int ok = log_open(LOGFILE, TD_LOG_APPEND, &log_errno);
        if (!ok) {
            QString msg = QStringLiteral("Cannot create %1 (errno=%2)")
                .arg(LOGFILE).arg(log_errno);
            qDebug() << msg;
            QMessageBox::warning(nullptr, "recovery-qt", msg);
        }
    }

    {
        int prev_pid = checkAndCleanupPrevious();
        if (prev_pid > 0) {
            QMessageBox::warning(nullptr, "recovery-qt",
                "Only one instance of recovery-qt can run at a time.");
            return 0;
        }
        if (!writeLock()) {
            QMessageBox::critical(nullptr, "recovery-qt",
                "Failed to create lock file " LOCKFILE);
            return 1;
        }
    }

    MainWindow window;
    window.show();

    int ret = app.exec();
    unlink(LOCKFILE);
    return ret;
}
