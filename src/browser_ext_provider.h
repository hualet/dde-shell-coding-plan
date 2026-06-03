// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "providerregistry.h"
#include "websocket_server.h"

#include <QMap>
#include <QObject>
#include <QTimer>

class BrowserExtProvider : public QObject
{
  Q_OBJECT

public:
  explicit BrowserExtProvider (WebSocketServer *wsServer,
                               QObject *parent = nullptr);

  void refreshProviders (const QStringList &providerIds);
  void openConsole (const QString &provider, const QString &url);

  bool isExtensionConnected () const;

signals:
  void refreshCompleted (const QString &providerId,
                         const QuotaSnapshot &snapshot);
  void refreshFailed (const QString &providerId,
                      const QString &message);
  void extensionStatusChanged (bool connected);
  void availableProvidersChanged (const QStringList &providers);

private slots:
  void onRefreshResult (const QString &requestId,
                        const QString &provider,
                        const QVariantMap &data);
  void onRefreshProgress (const QString &requestId,
                          const QString &provider,
                          const QString &status,
                          const QString &message);
  void onClientConnected ();
  void onClientDisconnected ();
  void onAuthSuccess ();
  void onAuthFailed ();
  void onRefreshTimeout ();
  void onStatusReceived (bool connected,
                         const QStringList &availableProviders);

private:
  QuotaSnapshot resultToSnapshot (const QString &providerId,
                                  const QVariantMap &data) const;
  void failPendingRequests ();

  WebSocketServer *m_wsServer;
  QString m_currentRequestId;
  QStringList m_pendingProviders;
  QMap<QString, int> m_requestTimeouts;
  QTimer *m_timeoutTimer;
  bool m_extensionReady = false;
};
