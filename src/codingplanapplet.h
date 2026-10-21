// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "applet.h"
#include "codingplanmodel.h"

DS_USE_NAMESPACE

class CodingPlanSettings;

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

  // Opens the DTK settings dialog (taskbar display options, manual account
  // management); called from the panel popup.
  Q_INVOKABLE void showSettings ();

private:
  CodingPlanModel *m_quotaModel = nullptr;
  CodingPlanSettings *m_settings = nullptr;
};
