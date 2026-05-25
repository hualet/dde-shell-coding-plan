// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "codingplanapplet.h"

#include "pluginfactory.h"

#include <QDebug>
#include <QFileInfo>
#include <QProcess>

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
  return DApplet::init ();
}

void
CodingPlanApplet::showSettings ()
{
  const QStringList candidates = {
    QStringLiteral ("/usr/bin/dde-coding-plan"),
    QStringLiteral ("/usr/local/bin/dde-coding-plan"),
    QStringLiteral ("dde-coding-plan"),
  };

  for (const QString &candidate : candidates)
    {
      if (candidate.contains (QLatin1Char ('/'))
          && !QFileInfo::exists (candidate))
        {
          continue;
        }

      if (QProcess::startDetached (candidate))
        {
          return;
        }
    }

  qWarning () << "Failed to launch dde-coding-plan settings window";
}

D_APPLET_CLASS (CodingPlanApplet)

#include "codingplanapplet.moc"
