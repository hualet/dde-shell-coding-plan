// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "browser_ext_provider.h"

#include <QUuid>
#include <QDebug>

namespace
{
constexpr int kProviderTimeoutMs = 30000;
}

BrowserExtProvider::BrowserExtProvider (WebSocketServer *wsServer,
                                        QObject *parent)
    : QObject (parent)
    , m_wsServer (wsServer)
    , m_timeoutTimer (new QTimer (this))
{
  m_timeoutTimer->setSingleShot (true);
  connect (m_timeoutTimer, &QTimer::timeout,
           this, &BrowserExtProvider::onRefreshTimeout);

  connect (m_wsServer, &WebSocketServer::refreshResultReceived,
           this, &BrowserExtProvider::onRefreshResult);
  connect (m_wsServer, &WebSocketServer::refreshProgressReceived,
           this, &BrowserExtProvider::onRefreshProgress);
  connect (m_wsServer, &WebSocketServer::clientConnected,
           this, &BrowserExtProvider::onClientConnected);
  connect (m_wsServer, &WebSocketServer::clientDisconnected,
           this, &BrowserExtProvider::onClientDisconnected);
  connect (m_wsServer, &WebSocketServer::authSuccess,
           this, &BrowserExtProvider::onAuthSuccess);
  connect (m_wsServer, &WebSocketServer::authFailed,
           this, &BrowserExtProvider::onAuthFailed);
  connect (m_wsServer, &WebSocketServer::statusReceived,
           this, &BrowserExtProvider::onStatusReceived);
}

void
BrowserExtProvider::refreshProviders (const QStringList &providerIds)
{
  if (!m_wsServer->isClientConnected ())
    {
      for (const QString &id : providerIds)
        {
          qWarning () << "[coding-plan][ext] cannot refresh" << id
                      << ": extension not connected";
          emit refreshFailed (id, QStringLiteral (
              "浏览器插件未连接，请安装并启动 DDE Coding Plan 浏览器扩展。"));
        }
      return;
    }

  m_pendingProviders = providerIds;
  m_currentRequestId = QUuid::createUuid ().toString (
      QUuid::WithoutBraces);

  m_wsServer->sendRefreshRequest (m_currentRequestId, providerIds,
                                  kProviderTimeoutMs);
  m_timeoutTimer->start (kProviderTimeoutMs * providerIds.size ());

  qInfo () << "[coding-plan][ext] refresh request sent"
           << m_currentRequestId << "providers" << providerIds;
}

void
BrowserExtProvider::openConsole (const QString &provider, const QString &url)
{
  if (!m_wsServer->isClientConnected ())
    {
      qWarning () << "[coding-plan][ext] cannot open console: extension not connected";
      return;
    }

  m_wsServer->sendOpenConsole (provider, url);
}

bool
BrowserExtProvider::isExtensionConnected () const
{
  return m_extensionReady;
}

void
BrowserExtProvider::onRefreshResult (const QString &requestId,
                                     const QString &provider,
                                     const QVariantMap &data)
{
  if (requestId != m_currentRequestId)
    {
      return;
    }

  const QuotaSnapshot snapshot = resultToSnapshot (provider, data);
  m_pendingProviders.removeOne (provider);

  qInfo () << "[coding-plan][ext] refresh result for" << provider
           << "status" << snapshotStatusToString (snapshot.status)
           << "ratio" << snapshot.remainingRatio;

  emit refreshCompleted (provider, snapshot);

  if (m_pendingProviders.isEmpty ())
    {
      m_timeoutTimer->stop ();
      m_currentRequestId.clear ();
    }
}

void
BrowserExtProvider::onRefreshProgress (const QString &requestId,
                                       const QString &provider,
                                       const QString &status,
                                       const QString &message)
{
  if (requestId != m_currentRequestId)
    {
      return;
    }

  qInfo () << "[coding-plan][ext] progress" << provider << status << message;
}

void
BrowserExtProvider::onClientConnected ()
{
  qInfo () << "[coding-plan][ext] client connected, waiting for auth";
}

void
BrowserExtProvider::onClientDisconnected ()
{
  m_extensionReady = false;
  failPendingRequests ();
  emit extensionStatusChanged (false);
}

void
BrowserExtProvider::onAuthSuccess ()
{
  m_extensionReady = true;
  emit extensionStatusChanged (true);
  qInfo () << "[coding-plan][ext] extension authenticated and ready";
}

void
BrowserExtProvider::onAuthFailed ()
{
  m_extensionReady = false;
  emit extensionStatusChanged (false);
  qWarning () << "[coding-plan][ext] extension auth failed";
}

void
BrowserExtProvider::onRefreshTimeout ()
{
  qWarning () << "[coding-plan][ext] refresh timed out for request"
              << m_currentRequestId;

  for (const QString &id : m_pendingProviders)
    {
      emit refreshFailed (id, QStringLiteral ("刷新超时，请检查浏览器插件是否正常。"));
    }

  m_pendingProviders.clear ();
  m_currentRequestId.clear ();
}

QuotaSnapshot
BrowserExtProvider::resultToSnapshot (const QString &providerId,
                                      const QVariantMap &data) const
{
  QuotaSnapshot snapshot;
  snapshot.providerId = providerId;
  snapshot.source = SourceType::BrowserExt;
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();

  const QString status
      = data.value (QStringLiteral ("status")).toString ();

  if (status == QStringLiteral ("ok"))
    {
      snapshot.status = SnapshotStatus::Ok;

      if (data.contains (QStringLiteral ("remainingRatio")))
        {
          bool ok = false;
          const double ratio
              = data.value (QStringLiteral ("remainingRatio")).toDouble (&ok);
          snapshot.remainingRatio = (ok && ratio >= 0)
              ? std::max (0.0, std::min (1.0, ratio)) : -1.0;
        }

      if (data.contains (QStringLiteral ("fiveHourRemainingRatio")))
        {
          bool ok = false;
          const double ratio
              = data.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (&ok);
          snapshot.fiveHourRemainingRatio = (ok && ratio >= 0)
              ? std::max (0.0, std::min (1.0, ratio)) : -1.0;
        }

      snapshot.balanceText
          = data.value (QStringLiteral ("balanceText")).toString ();
      snapshot.fiveHourBalanceText
          = data.value (QStringLiteral ("fiveHourBalanceText")).toString ();
      snapshot.unit = data.value (QStringLiteral ("unit")).toString ();
      snapshot.consoleUrl
          = data.value (QStringLiteral ("consoleUrl")).toString ();

      if (data.contains (QStringLiteral ("used")))
        {
          snapshot.used
              = data.value (QStringLiteral ("used")).toDouble ();
        }
      if (data.contains (QStringLiteral ("total")))
        {
          snapshot.total
              = data.value (QStringLiteral ("total")).toDouble ();
        }
    }
  else if (status == QStringLiteral ("auth_error"))
    {
      snapshot.status = SnapshotStatus::AuthError;
      snapshot.message = data.value (QStringLiteral ("message")).toString ();
      if (snapshot.message.isEmpty ())
        {
          snapshot.message = QStringLiteral ("登录已过期，请重新登录。");
        }
    }
  else if (status == QStringLiteral ("parse_error"))
    {
      snapshot.status = SnapshotStatus::ParseError;
      snapshot.message = data.value (QStringLiteral ("message")).toString ();
      if (snapshot.message.isEmpty ())
        {
          snapshot.message = QStringLiteral ("无法解析额度页面。");
        }
    }
  else if (status == QStringLiteral ("network_error"))
    {
      snapshot.status = SnapshotStatus::NetworkError;
      snapshot.message = data.value (QStringLiteral ("message")).toString ();
      if (snapshot.message.isEmpty ())
        {
          snapshot.message = QStringLiteral ("网络错误。");
        }
    }
  else
    {
      snapshot.status = SnapshotStatus::Unsupported;
      snapshot.message = data.value (QStringLiteral ("message")).toString ();
      if (snapshot.message.isEmpty ())
        {
          snapshot.message = QStringLiteral ("未知错误。");
        }
    }

  return snapshot;
}

void
BrowserExtProvider::onStatusReceived (bool connected,
                                      const QStringList &availableProviders)
{
  Q_UNUSED (connected)
  emit availableProvidersChanged (availableProviders);
}

void
BrowserExtProvider::failPendingRequests ()
{
  for (const QString &id : m_pendingProviders)
    {
      emit refreshFailed (id, QStringLiteral (
          "浏览器插件已断开连接。"));
    }
  m_pendingProviders.clear ();
  m_currentRequestId.clear ();
  m_timeoutTimer->stop ();
}
