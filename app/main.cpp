// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webbridge.h"

#include <DApplication>
#include <DMainWindow>
#include <DWidgetUtil>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWebChannel>
#include <QUrl>
#include <QShortcut>
#include <QKeySequence>
#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QTimer>

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

        m_profile = new QWebEngineProfile(QStringLiteral("coding-plan-standalone"), this);
        m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

        m_webView = new QWebEngineView(this);
        setCentralWidget(m_webView);

        auto *page = new QWebEnginePage(m_profile, this);
        m_webView->setPage(page);
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

        connect(m_webView, &QWebEngineView::loadFinished, this, &MainWindow::onPageLoadFinished);
        connect(m_bridge, &WebBridge::loginPageRequested, this, &MainWindow::onLoginPageRequested);
        connect(m_bridge, &WebBridge::loginFinished, this, &MainWindow::onLoginFinished);
    }

    void loadFrontend()
    {
        const QStringList frontendPaths {
            qApp->applicationDirPath() + QStringLiteral("/web/index.html"),
            QStringLiteral(CODING_PLAN_WEB_DIR "/index.html"),
            QStringLiteral("/usr/share/dde-coding-plan/web/index.html"),
            QStringLiteral("/usr/local/share/dde-coding-plan/web/index.html")
        };

        for (const QString &frontendPath : frontendPaths) {
            if (QFile::exists(frontendPath)) {
                m_webView->setUrl(QUrl::fromLocalFile(frontendPath));
                m_reactUrl = m_webView->url().toString();
                return;
            }
        }

        qWarning() << "Could not find React frontend in" << frontendPaths;
        m_webView->setHtml(tr("Coding Plan frontend is not installed."));
    }

private slots:
    void onLoginPageRequested(const QString &providerId, const QString &loginUrl)
    {
        m_loginProviderId = providerId;
        m_loginProviderConfig = m_bridge->getProviderConfig(providerId);
        m_loginPhase = 0;
        m_loginQuotaProbeStarted = false;
        m_webView->setUrl(QUrl(loginUrl));
    }

    void onLoginFinished(const QString &providerId)
    {
        Q_UNUSED(providerId)
        if (!m_reactUrl.isEmpty()) {
            m_webView->setUrl(QUrl(m_reactUrl));
        }
    }

    void onPageLoadFinished(bool ok)
    {
        if (!ok || m_loginProviderId.isEmpty()) {
            return;
        }

        if (!isAllowedOrigin(m_webView->url())) {
            return;
        }

        const QString currentPath = m_webView->url().path();
        const QString loginPath = m_loginProviderConfig.value(QStringLiteral("loginUrl")).toString();
        const QString quotaUrl = m_loginProviderConfig.value(QStringLiteral("quotaUrl")).toString();
        const QString consoleUrl = m_loginProviderConfig.value(QStringLiteral("consoleUrl")).toString();

        const bool onLoginPage = (currentPath.indexOf(QStringLiteral("/login")) >= 0 ||
                                   currentPath == QUrl(loginPath).path());

        if (onLoginPage && m_loginPhase == 0) {
            m_loginPhase = 1;
            if (!quotaUrl.isEmpty() && !m_loginQuotaProbeStarted) {
                m_loginQuotaProbeStarted = true;
                QTimer::singleShot(1200, this, [this, quotaUrl]() {
                    if (!m_loginProviderId.isEmpty() && m_loginPhase == 1) {
                        m_webView->setUrl(QUrl(quotaUrl));
                    }
                });
            }
            return;
        }

        if (onLoginPage) {
            return;
        }

        if (m_loginPhase == 0) {
            m_loginPhase = 1;
        }

        auto pathMatch = [](const QString &urlStr, const QString &urlToMatch) -> bool {
            if (urlToMatch.isEmpty())
                return false;
            const QString path = QUrl(urlToMatch).path();
            if (path.isEmpty() || path == QStringLiteral("/"))
                return urlStr.indexOf(QUrl(urlToMatch).host()) >= 0;
            return urlStr.indexOf(path) >= 0;
        };

        const bool onQuotaPage = !quotaUrl.isEmpty() &&
            (pathMatch(m_webView->url().toString(), quotaUrl) ||
             pathMatch(m_webView->url().toString(), consoleUrl));

        if (m_loginPhase == 1 && !onQuotaPage) {
            m_loginPhase = 2;
            if (!quotaUrl.isEmpty()) {
                m_webView->setUrl(QUrl(quotaUrl));
            }
            return;
        }

        const QString extractorScript = m_loginProviderConfig.value(QStringLiteral("extractorScript")).toString();
        if (extractorScript.trimmed().isEmpty()) {
            return;
        }

        QTimer::singleShot(1500, this, [this, extractorScript]() {
            m_webView->page()->runJavaScript(extractorScript, [this](const QVariant &result) {
                const QVariantMap data = result.toMap();
                const QString providerId = m_loginProviderId;

                if (data.value(QStringLiteral("status")).toString() == QStringLiteral("ok")) {
                    m_bridge->setWebViewResult(providerId, data);
                } else {
                    m_bridge->setProviderError(providerId,
                                               data.value(QStringLiteral("message")).toString());
                }

                m_loginProviderId.clear();
                m_loginProviderConfig.clear();
                m_loginPhase = 0;
                m_loginQuotaProbeStarted = false;
                onLoginFinished(providerId);
            });
        });
    }

private:
    bool isAllowedOrigin(const QUrl &url) const
    {
        const QString pageOrigin = url.scheme() + QStringLiteral("://") + url.host();
        const QStringList allowedOrigins = m_loginProviderConfig.value(QStringLiteral("allowedOrigins")).toStringList();
        return allowedOrigins.contains(pageOrigin);
    }

    QWebEngineView *m_webView = nullptr;
    QWebEngineProfile *m_profile = nullptr;
    QWebChannel *m_channel = nullptr;
    WebBridge *m_bridge = nullptr;
    QString m_reactUrl;
    QString m_loginProviderId;
    QVariantMap m_loginProviderConfig;
    int m_loginPhase = 0;
    bool m_loginQuotaProbeStarted = false;
};

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);
    a.setOrganizationName(QStringLiteral("deepin"));
    a.setApplicationName(QStringLiteral("dde-shell-coding-plan"));
    a.setApplicationVersion(QStringLiteral("0.2.0"));
    a.setProductName(QObject::tr("Coding Plan"));
    a.setProductIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));

    MainWindow w;
    w.show();
    w.loadFrontend();

    Dtk::Widget::moveToCenter(&w);
    return a.exec();
}

#include "main.moc"
