// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "codingplanapplet.h"
#include "websocket_server.h"

#include "pluginfactory.h"

#include <QDebug>

CodingPlanApplet::CodingPlanApplet (QObject *parent) : DApplet (parent)
{
  qDebug () << "CodingPlanApplet created";
}

CodingPlanApplet::~CodingPlanApplet ()
{
  qDebug () << "CodingPlanApplet destroyed";
}

bool
CodingPlanApplet::init ()
{
  m_quotaModel = new CodingPlanModel (this);

  m_wsServer = new WebSocketServer (this);
  if (m_wsServer->start ())
    {
      m_quotaModel->setWebSocketServer (m_wsServer);
    }
  else
    {
      qWarning () << "[coding-plan] failed to start WebSocket server";
    }

  m_quotaModel->watchExternalChanges ();
  return DApplet::init ();
}

D_APPLET_CLASS (CodingPlanApplet)

#include "codingplanapplet.moc"
