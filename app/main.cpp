// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webbridge.h"

#include <DApplication>
#include <DMainWindow>
#include <DWidgetUtil>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWebChannel>
#include <QShortcut>
#include <QKeySequence>
#include <QDebug>

DWIDGET_USE_NAMESPACE

class MainWindow : public DMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr)
        : DMainWindow(parent)
    {
        setWindowTitle(tr("Coding Plan"));
        resize(420, 720);
        setMinimumSize(360, 600);

        m_webView = new QWebEngineView(this);
        setCentralWidget(m_webView);

        m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
        m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);

        m_channel = new QWebChannel(this);
        m_bridge = new WebBridge(this);
        m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
        m_webView->page()->setWebChannel(m_channel);

        QShortcut *refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
        connect(refreshShortcut, &QShortcut::activated, this, [this]() {
            m_webView->reload();
        });

        connect(m_bridge, &WebBridge::loginPageRequested, this, &MainWindow::onLoginPageRequested);
        connect(m_bridge, &WebBridge::loginFinished, this, &MainWindow::onLoginFinished);
    }

    void loadFrontend()
    {
        QString frontendPath = qApp->applicationDirPath() + QStringLiteral("/web/index.html");
        if (QFile::exists(frontendPath)) {
            m_webView->setUrl(QUrl::fromLocalFile(frontendPath));
        } else {
            m_webView->setUrl(QUrl(QStringLiteral("qrc:/web/index.html")));
        }
        m_reactUrl = m_webView->url().toString();
    }

private slots:
    void onLoginPageRequested(const QString &providerId, const QString &loginUrl)
    {
        m_loginProviderId = providerId;
        m_webView->setUrl(QUrl(loginUrl));
    }

    void onLoginFinished(const QString &providerId)
    {
        Q_UNUSED(providerId)
        if (!m_reactUrl.isEmpty()) {
            m_webView->setUrl(QUrl(m_reactUrl));
        }
    }

private:
    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_channel = nullptr;
    WebBridge *m_bridge = nullptr;
    QString m_reactUrl;
    QString m_loginProviderId;
};

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);
    a.setOrganizationName(QStringLiteral("deepin"));
    a.setApplicationName(QStringLiteral("dde-coding-plan"));
    a.setApplicationVersion(QStringLiteral("0.2.0"));
    a.setProductName(QObject::tr("Coding Plan"));
    a.setProductIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));
    a.loadTranslator();

    MainWindow w;
    w.show();
    w.loadFrontend();

    Dtk::Widget::moveToCenter(&w);
    return a.exec();
}

#include "main.moc"
