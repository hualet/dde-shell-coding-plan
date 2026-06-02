// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QVariantMap>

class QTimer;
class QWebSocketServer;
class QWebSocket;

class WebSocketServer : public QObject
{
  Q_OBJECT

public:
  explicit WebSocketServer (QObject *parent = nullptr);
  ~WebSocketServer () override;

  bool start (quint16 port = 18765);
  void stop ();

  bool isListening () const;
  bool isClientConnected () const;

  void sendRefreshRequest (const QString &requestId,
                           const QStringList &providers,
                           int timeout = 15000);
  void sendOpenConsole (const QString &provider, const QString &url);

  void generateToken ();
  QString token () const;

signals:
  void clientConnected ();
  void clientDisconnected ();
  void authSuccess ();
  void authFailed ();
  void refreshResultReceived (const QString &requestId,
                              const QString &provider,
                              const QVariantMap &data);
  void refreshProgressReceived (const QString &requestId,
                                const QString &provider,
                                const QString &status,
                                const QString &message);
  void statusReceived (bool connected,
                       const QStringList &availableProviders);

private slots:
  void onNewConnection ();
  void onTextMessageReceived (const QString &message);
  void onClientDisconnected ();
  void onHeartbeatTimeout ();

private:
  void handleAuthMessage (const QVariantMap &msg);
  void handleStatusMessage (const QVariantMap &msg);
  void handleRefreshResultMessage (const QVariantMap &msg);
  void handleRefreshProgressMessage (const QVariantMap &msg);
  void sendJson (const QVariantMap &msg);
  void startHeartbeat ();
  void stopHeartbeat ();

  QWebSocketServer *m_server = nullptr;
  QWebSocket *m_client = nullptr;
  QString m_token;
  bool m_authenticated = false;
  QTimer *m_heartbeatTimer = nullptr;
};
