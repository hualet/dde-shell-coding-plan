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

class FilteringWebEnginePage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit FilteringWebEnginePage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent)
    {
    }

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override
    {
        if (isExpectedConsoleNoise(message)) {
            return;
        }

        QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
    }

private:
    static bool isExpectedConsoleNoise(const QString &message)
    {
        return message.contains(QStringLiteral("DialogContent` requires a `DialogTitle"))
            || message.contains(QStringLiteral("Unrecognized feature: 'ch-ua-form-factors'"))
            || message.contains(QStringLiteral("Found a 'popover' attribute with an invalid value"))
            || message.contains(QStringLiteral("RequestError: Failed to fetch"))
            || message.contains(QStringLiteral("Failed to fetch"))
            || (message.contains(QStringLiteral("DOMException"))
                && message.contains(QStringLiteral("[object Object]")))
            || message.contains(QStringLiteral("RecoverableError: Minified React error #418"));
    }
};

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

        auto *page = new FilteringWebEnginePage(m_profile, this);
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
        connect(m_bridge, &WebBridge::refreshAllRequested, this, &MainWindow::onRefreshAllRequested);
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
        qWarning() << "[coding-plan][standalone] login/extract requested"
                 << providerId << loginUrl;
        m_loginProviderId = providerId;
        m_loginProviderConfig = m_bridge->getProviderConfig(providerId);
        m_loginPhase = 0;
        m_loginQuotaProbeStarted = false;
        m_extractionAttempts = 0;
        m_webView->setUrl(QUrl(loginUrl));
    }

    void onLoginFinished(const QString &providerId)
    {
        Q_UNUSED(providerId)
        if (!m_reactUrl.isEmpty()) {
            m_webView->setUrl(QUrl(m_reactUrl));
        }

        if (!m_refreshQueue.isEmpty()) {
            QTimer::singleShot(500, this, &MainWindow::refreshNextProvider);
        }
    }

    void onRefreshAllRequested()
    {
        qWarning() << "[coding-plan][standalone] refresh all requested";
        m_refreshQueue.clear();
        const QVariantList providers = m_bridge->providers();
        for (const QVariant &item : providers) {
            const QVariantMap provider = item.toMap();
            const QString providerId = provider.value(QStringLiteral("id")).toString();
            const QString quotaUrl = provider.value(QStringLiteral("quotaUrl")).toString();
            if (!providerId.isEmpty() && !quotaUrl.isEmpty()) {
                m_refreshQueue.append(providerId);
            }
        }
        refreshNextProvider();
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

        m_extractionAttempts = 0;
        QTimer::singleShot(1500, this, [this, extractorScript]() {
            runExtractorWithRetry(extractorScript);
        });
    }

private:
    void refreshNextProvider()
    {
        if (m_refreshQueue.isEmpty() || !m_loginProviderId.isEmpty()) {
            return;
        }

        const QString providerId = m_refreshQueue.takeFirst();
        const QVariantMap provider = m_bridge->getProviderConfig(providerId);
        const QString quotaUrl = provider.value(QStringLiteral("quotaUrl")).toString();
        if (quotaUrl.isEmpty()) {
            refreshNextProvider();
            return;
        }

        onLoginPageRequested(providerId, quotaUrl);
    }

    void runExtractorWithRetry(const QString &extractorScript)
    {
        if (m_loginProviderId.isEmpty()) {
            return;
        }

        m_extractionAttempts += 1;
        qWarning() << "[coding-plan][standalone] extract attempt"
                 << m_loginProviderId
                 << "attempt" << m_extractionAttempts
                 << "url" << m_webView->url().toString();
        m_webView->page()->runJavaScript(extractorScript, [this, extractorScript](const QVariant &result) {
            const QVariantMap data = result.toMap();
            const QString status = data.value(QStringLiteral("status")).toString();
            qWarning() << "[coding-plan][standalone] extract result"
                     << m_loginProviderId
                     << "attempt" << m_extractionAttempts
                     << "status" << status
                     << "remaining" << data.value(QStringLiteral("remainingRatio"))
                     << "fiveHour" << data.value(QStringLiteral("fiveHourRemainingRatio"))
                     << "balanceText" << data.value(QStringLiteral("balanceText")).toString()
                     << "fiveHourText" << data.value(QStringLiteral("fiveHourBalanceText")).toString()
                     << "message" << data.value(QStringLiteral("message")).toString();

            if (status == QStringLiteral("ok")) {
                const QString providerId = m_loginProviderId;
                m_bridge->setWebViewResult(providerId, data);
                finishExtraction(providerId);
                return;
            }

            if (m_extractionAttempts < maxExtractionAttempts()) {
                qWarning() << "[coding-plan][standalone] extract retry scheduled"
                         << m_loginProviderId
                         << "attempt" << m_extractionAttempts;
                QTimer::singleShot(1200, this, [this, extractorScript]() {
                    runExtractorWithRetry(extractorScript);
                });
                return;
            }

            const QString providerId = m_loginProviderId;
            m_bridge->setProviderError(providerId,
                                       data.value(QStringLiteral("message")).toString());
            finishExtraction(providerId);
        });
    }

    void finishExtraction(const QString &providerId)
    {
        qWarning() << "[coding-plan][standalone] finish extraction" << providerId;
        m_loginProviderId.clear();
        m_loginProviderConfig.clear();
        m_loginPhase = 0;
        m_loginQuotaProbeStarted = false;
        m_extractionAttempts = 0;
        onLoginFinished(providerId);
    }

    static int maxExtractionAttempts()
    {
        return 6;
    }

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
    QStringList m_refreshQueue;
    int m_loginPhase = 0;
    int m_extractionAttempts = 0;
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
