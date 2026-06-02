// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "websocket_server.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QUuid>
#include <QRandomGenerator>
#include <QDir>
#include <QTimer>
#include <QSaveFile>

namespace
{
constexpr quint16 kDefaultPort = 18765;
constexpr int kTokenLength = 32;
constexpr int kHeartbeatIntervalMs = 30000;
constexpr int kHeartbeatTimeoutMs = 35000;

QString
tokenFilePath ()
{
  return QStandardPaths::writableLocation (QStandardPaths::ConfigLocation)
         + QStringLiteral ("/dde-shell-coding-plan/ext-token");
}
}

WebSocketServer::WebSocketServer (QObject *parent)
    : QObject (parent)
    , m_heartbeatTimer (new QTimer (this))
{
  m_server = new QWebSocketServer (
      QStringLiteral ("dde-coding-plan"),
      QWebSocketServer::NonSecureMode,
      this);

  m_heartbeatTimer->setSingleShot (true);
  connect (m_heartbeatTimer, &QTimer::timeout,
           this, &WebSocketServer::onHeartbeatTimeout);
}

WebSocketServer::~WebSocketServer ()
{
  stop ();
}

bool
WebSocketServer::start (quint16 port)
{
  if (m_server->isListening ())
    {
      return true;
    }

  const bool ok = m_server->listen (QHostAddress::LocalHost, port);
  if (!ok)
    {
      qWarning () << "[coding-plan][ws] failed to listen on port" << port
                  << m_server->errorString ();
      return false;
    }

  qInfo () << "[coding-plan][ws] listening on 127.0.0.1:" << port;

  connect (m_server, &QWebSocketServer::newConnection,
           this, &WebSocketServer::onNewConnection);

  generateToken ();

  return true;
}

void
WebSocketServer::stop ()
{
  discardClient ();
  m_server->close ();
  m_authenticated = false;
}

bool
WebSocketServer::isListening () const
{
  return m_server->isListening ();
}

bool
WebSocketServer::isClientConnected () const
{
  return m_client != nullptr && m_authenticated;
}

void
WebSocketServer::generateToken ()
{
  QFile file (tokenFilePath ());
  if (file.exists () && file.open (QIODevice::ReadOnly))
    {
      m_token = QString::fromUtf8 (file.readAll ().trimmed ());
      if (m_token.length () >= kTokenLength)
        {
          qInfo () << "[coding-plan][ws] loaded existing token";
          return;
        }
    }

  QByteArray tokenBytes;
  tokenBytes.reserve (kTokenLength);
  QRandomGenerator *rng = QRandomGenerator::global ();
  for (int i = 0; i < kTokenLength; ++i)
    {
      quint8 byte = static_cast<quint8> (rng->bounded (256));
      tokenBytes.append (byte);
    }
  m_token = QString::fromLatin1 (tokenBytes.toHex ());

  QDir ().mkpath (QFileInfo (tokenFilePath ()).absolutePath ());
  QSaveFile saveFile (tokenFilePath ());
  saveFile.setPermissions (QFile::ReadOwner | QFile::WriteOwner);
  if (saveFile.open (QIODevice::WriteOnly | QIODevice::Truncate))
    {
      saveFile.write (m_token.toUtf8 ());
      if (saveFile.commit ())
        {
          qInfo () << "[coding-plan][ws] generated new token (permissions 0600)";
        }
      else
        {
          qWarning () << "[coding-plan][ws] failed to commit token file";
        }
    }
  else
    {
      qWarning () << "[coding-plan][ws] failed to save token to"
                  << tokenFilePath ();
    }
}

QString
WebSocketServer::token () const
{
  return m_token;
}

void
WebSocketServer::sendRefreshRequest (const QString &requestId,
                                     const QStringList &providers,
                                     int timeout)
{
  QVariantMap msg;
  msg.insert (QStringLiteral ("type"), QStringLiteral ("refresh_request"));
  msg.insert (QStringLiteral ("requestId"), requestId);
  msg.insert (QStringLiteral ("providers"), providers);
  msg.insert (QStringLiteral ("timeout"), timeout);
  sendJson (msg);
}

void
WebSocketServer::sendOpenConsole (const QString &provider, const QString &url)
{
  QVariantMap msg;
  msg.insert (QStringLiteral ("type"), QStringLiteral ("open_console"));
  msg.insert (QStringLiteral ("provider"), provider);
  msg.insert (QStringLiteral ("url"), url);
  sendJson (msg);
}

void
WebSocketServer::onNewConnection ()
{
  QWebSocket *newClient = m_server->nextPendingConnection ();

  if (m_client)
    {
      qWarning () << "[coding-plan][ws] closing existing connection before accepting new one";
      discardClient ();
      emit clientDisconnected ();
    }

  m_client = newClient;
  m_authenticated = false;

  connect (m_client, &QWebSocket::textMessageReceived,
           this, &WebSocketServer::onTextMessageReceived);
  connect (m_client, &QWebSocket::disconnected,
           this, &WebSocketServer::onClientDisconnected);

  qInfo () << "[coding-plan][ws] new connection from"
           << m_client->peerAddress ().toString ();

  startHeartbeat ();

  emit clientConnected ();
}

void
WebSocketServer::onTextMessageReceived (const QString &message)
{
  const QJsonDocument doc = QJsonDocument::fromJson (message.toUtf8 ());
  if (doc.isNull () || !doc.isObject ())
    {
      qWarning () << "[coding-plan][ws] invalid JSON message";
      return;
    }

  const QVariantMap msg = doc.object ().toVariantMap ();
  const QString type = msg.value (QStringLiteral ("type")).toString ();

  if (type == QStringLiteral ("auth"))
    {
      m_heartbeatTimer->start (kHeartbeatTimeoutMs);
      handleAuthMessage (msg);
      return;
    }

  if (type == QStringLiteral ("heartbeat"))
    {
      m_heartbeatTimer->start (kHeartbeatTimeoutMs);
      return;
    }

  if (!m_authenticated)
    {
      qWarning () << "[coding-plan][ws] rejecting unauthenticated message:" << type;
      return;
    }

  m_heartbeatTimer->start (kHeartbeatTimeoutMs);

  if (type == QStringLiteral ("status"))
    {
      handleStatusMessage (msg);
    }
  else if (type == QStringLiteral ("refresh_result"))
    {
      handleRefreshResultMessage (msg);
    }
  else if (type == QStringLiteral ("refresh_progress"))
    {
      handleRefreshProgressMessage (msg);
    }
  else
    {
      qWarning () << "[coding-plan][ws] unknown message type" << type;
    }
}

void
WebSocketServer::onClientDisconnected ()
{
  if (!m_client)
    {
      return;
    }

  qInfo () << "[coding-plan][ws] client disconnected";
  stopHeartbeat ();

  QWebSocket *old = m_client;
  m_client = nullptr;
  m_authenticated = false;

  disconnect (old, &QWebSocket::textMessageReceived,
              this, &WebSocketServer::onTextMessageReceived);
  disconnect (old, &QWebSocket::disconnected,
              this, &WebSocketServer::onClientDisconnected);
  old->deleteLater ();

  emit clientDisconnected ();
}

void
WebSocketServer::handleAuthMessage (const QVariantMap &msg)
{
  const QString clientToken
      = msg.value (QStringLiteral ("token")).toString ();

  if (clientToken == m_token)
    {
      m_authenticated = true;
      startHeartbeat ();
      qInfo () << "[coding-plan][ws] auth success";

      QVariantMap reply;
      reply.insert (QStringLiteral ("type"), QStringLiteral ("auth_result"));
      reply.insert (QStringLiteral ("success"), true);
      sendJson (reply);

      emit authSuccess ();
    }
  else
    {
      qWarning () << "[coding-plan][ws] auth failed: token mismatch";

      QVariantMap reply;
      reply.insert (QStringLiteral ("type"), QStringLiteral ("auth_result"));
      reply.insert (QStringLiteral ("success"), false);
      reply.insert (QStringLiteral ("message"), QStringLiteral ("Token mismatch"));

      if (m_client)
        {
          const QByteArray data = QJsonDocument (
              QJsonObject::fromVariantMap (reply)).toJson (QJsonDocument::Compact);
          m_client->sendTextMessage (QString::fromUtf8 (data));
        }

      m_authenticated = false;
      emit authFailed ();
      discardClient ();
    }
}

void
WebSocketServer::handleStatusMessage (const QVariantMap &msg)
{
  const bool connected
      = msg.value (QStringLiteral ("connected")).toBool ();
  const QStringList providers
      = msg.value (QStringLiteral ("availableProviders")).toStringList ();
  emit statusReceived (connected, providers);
}

void
WebSocketServer::handleRefreshResultMessage (const QVariantMap &msg)
{
  const QString requestId
      = msg.value (QStringLiteral ("requestId")).toString ();
  const QString provider
      = msg.value (QStringLiteral ("provider")).toString ();
  const QVariantMap data
      = msg.value (QStringLiteral ("data")).toMap ();
  emit refreshResultReceived (requestId, provider, data);
}

void
WebSocketServer::handleRefreshProgressMessage (const QVariantMap &msg)
{
  const QString requestId
      = msg.value (QStringLiteral ("requestId")).toString ();
  const QString provider
      = msg.value (QStringLiteral ("provider")).toString ();
  const QString status
      = msg.value (QStringLiteral ("status")).toString ();
  const QString message
      = msg.value (QStringLiteral ("message")).toString ();
  emit refreshProgressReceived (requestId, provider, status, message);
}

void
WebSocketServer::sendJson (const QVariantMap &msg)
{
  if (!m_client || !m_authenticated)
    {
      qWarning () << "[coding-plan][ws] cannot send: not connected/authenticated";
      return;
    }

  const QByteArray data = QJsonDocument (
      QJsonObject::fromVariantMap (msg)).toJson (QJsonDocument::Compact);
  m_client->sendTextMessage (QString::fromUtf8 (data));
}

void
WebSocketServer::startHeartbeat ()
{
  m_heartbeatTimer->start (kHeartbeatTimeoutMs);
}

void
WebSocketServer::stopHeartbeat ()
{
  m_heartbeatTimer->stop ();
}

void
WebSocketServer::discardClient ()
{
  if (!m_client)
    {
      return;
    }

  stopHeartbeat ();

  QWebSocket *old = m_client;
  m_client = nullptr;
  m_authenticated = false;

  disconnect (old, &QWebSocket::textMessageReceived,
              this, &WebSocketServer::onTextMessageReceived);
  disconnect (old, &QWebSocket::disconnected,
              this, &WebSocketServer::onClientDisconnected);
  old->close (QWebSocketProtocol::CloseCodeNormal,
              QStringLiteral ("replaced"));
  old->deleteLater ();
}

void
WebSocketServer::onHeartbeatTimeout ()
{
  qWarning () << "[coding-plan][ws] heartbeat timeout, closing connection";
  discardClient ();
}
