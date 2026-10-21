// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "codingplanmodel.h"

#include <QPointer>
#include <QWidget>

namespace Dtk {
namespace Core {
class DSettings;
} // namespace Core
namespace Widget {
class DSettingsDialog;
} // namespace Widget
} // namespace Dtk

class QComboBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

// "显示 Plan" option widget: one row per account (auto-detected and manual),
// each with a switch that controls whether the plan shows up on the taskbar.
class PlanVisibilityWidget : public QWidget
{
  Q_OBJECT

public:
  explicit PlanVisibilityWidget (CodingPlanModel *model, QWidget *parent = nullptr);

private slots:
  void rebuildPlanRows ();

private:
  CodingPlanModel *m_model = nullptr;
  QVBoxLayout *m_rowsLayout = nullptr;
};

// "添加 Plan" option widget: lists manually added accounts (each with a
// remove button) and hosts the add-account form that used to live in the
// taskbar popup. Auto-detected CLI accounts are managed by their CLIs and
// are not listed here.
class ManualAccountsWidget : public QWidget
{
  Q_OBJECT

public:
  explicit ManualAccountsWidget (CodingPlanModel *model, QWidget *parent = nullptr);

private slots:
  void refreshVendorBox ();
  void rebuildAccountRows ();
  void onAddClicked ();

private:
  CodingPlanModel *m_model = nullptr;
  QVBoxLayout *m_rowsLayout = nullptr;
  QComboBox *m_vendorBox = nullptr;
  QLineEdit *m_labelEdit = nullptr;
  QLineEdit *m_keyEdit = nullptr;
  QPushButton *m_addButton = nullptr;
};

// DTK settings dialog for the applet: a DSettings schema (two groups,
// "显示 Plan" and "添加 Plan") shown through DSettingsDialog. Both option
// widgets are pure views over CodingPlanModel, which owns all persistence
// (including per-entry taskbar visibility).
class CodingPlanSettings : public QObject
{
  Q_OBJECT

public:
  explicit CodingPlanSettings (CodingPlanModel *model, QObject *parent = nullptr);
  ~CodingPlanSettings () override;

  // Shows the settings dialog, or raises the existing one.
  void showDialog ();

  static QByteArray buildSchema ();

private:
  CodingPlanModel *m_model = nullptr;
  Dtk::Core::DSettings *m_settings = nullptr;
  QPointer<Dtk::Widget::DSettingsDialog> m_dialog;
};
