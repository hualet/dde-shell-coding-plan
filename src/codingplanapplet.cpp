// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "codingplanapplet.h"

#include "codingplansettings.h"
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
  m_settings = new CodingPlanSettings (m_quotaModel, this);
  m_quotaModel->startAutoRefresh ();
  return DApplet::init ();
}

void
CodingPlanApplet::showSettings ()
{
  if (m_settings)
    {
      m_settings->showDialog ();
    }
}

D_APPLET_CLASS (CodingPlanApplet)

#include "codingplanapplet.moc"
