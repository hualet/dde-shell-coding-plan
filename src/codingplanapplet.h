// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "applet.h"
#include "codingplanmodel.h"

DS_USE_NAMESPACE

class WebSocketServer;

class CodingPlanApplet : public DApplet
{
  Q_OBJECT
  Q_PROPERTY (CodingPlanModel *quota READ quota CONSTANT)

public:
  explicit CodingPlanApplet (QObject *parent = nullptr);
  ~CodingPlanApplet () override;

  bool init () override;

  CodingPlanModel *
  quota () const
  {
    return m_quotaModel;
  }

private:
  CodingPlanModel *m_quotaModel = nullptr;
  WebSocketServer *m_wsServer = nullptr;
};
