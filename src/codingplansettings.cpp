// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "codingplansettings.h"

#include <DSettings>
#include <DSettingsDialog>
#include <DSwitchButton>
#include <dsettingswidgetfactory.h>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariantList>
#include <QVBoxLayout>

namespace {
constexpr auto kPlanListViewType = "codingplan-planlist";
constexpr auto kAccountsViewType = "codingplan-accounts";
} // namespace

namespace {

QString
planTitle (const QVariantMap &snapshot)
{
  QString title = snapshot.value (QStringLiteral ("providerName")).toString ();
  const QString label = snapshot.value (QStringLiteral ("label")).toString ();
  if (!label.isEmpty ())
    {
      title += QStringLiteral (" · ") + label;
    }
  return title;
}

} // namespace

PlanVisibilityWidget::PlanVisibilityWidget (CodingPlanModel *model,
                                            QWidget *parent)
    : QWidget (parent), m_model (model)
{
  auto *rootLayout = new QVBoxLayout (this);
  rootLayout->setContentsMargins (0, 0, 0, 0);
  rootLayout->setSpacing (8);

  auto *note = new QLabel (tr ("Plans switched on are shown on the taskbar"), this);
  note->setWordWrap (true);
  rootLayout->addWidget (note);

  m_rowsLayout = new QVBoxLayout ();
  m_rowsLayout->setSpacing (4);
  rootLayout->addLayout (m_rowsLayout);

  connect (m_model, &CodingPlanModel::snapshotsChanged, this,
           &PlanVisibilityWidget::rebuildPlanRows);
  rebuildPlanRows ();
}

void
PlanVisibilityWidget::rebuildPlanRows ()
{
  while (m_rowsLayout->count () > 0)
    {
      QLayoutItem *item = m_rowsLayout->takeAt (0);
      delete item->widget ();
      delete item;
    }

  const QVariantList snapshots = m_model->snapshots ();
  for (const QVariant &item : snapshots)
    {
      const QVariantMap snapshot = item.toMap ();
      const QString entryId = snapshot.value (QStringLiteral ("entryId")).toString ();

      auto *row = new QWidget (this);
      auto *rowLayout = new QHBoxLayout (row);
      rowLayout->setContentsMargins (0, 0, 0, 0);
      auto *nameLabel = new QLabel (planTitle (snapshot), row);
      auto *switchButton = new Dtk::Widget::DSwitchButton (row);
      switchButton->setChecked (
          snapshot.value (QStringLiteral ("visible")).toBool ());
      rowLayout->addWidget (nameLabel, 1);
      rowLayout->addWidget (switchButton);
      connect (switchButton, &Dtk::Widget::DSwitchButton::toggled, this,
               [this, entryId] (bool checked) {
                 m_model->setEntryVisible (entryId, checked);
               });
      m_rowsLayout->addWidget (row);
    }
}

ManualAccountsWidget::ManualAccountsWidget (CodingPlanModel *model,
                                            QWidget *parent)
    : QWidget (parent), m_model (model)
{
  auto *rootLayout = new QVBoxLayout (this);
  rootLayout->setContentsMargins (0, 0, 0, 0);
  rootLayout->setSpacing (8);

  m_rowsLayout = new QVBoxLayout ();
  m_rowsLayout->setSpacing (4);
  rootLayout->addLayout (m_rowsLayout);

  m_vendorBox = new QComboBox (this);
  m_labelEdit = new QLineEdit (this);
  m_labelEdit->setPlaceholderText (tr ("Label (optional)"));
  m_keyEdit = new QLineEdit (this);
  m_keyEdit->setPlaceholderText (tr ("Paste API Key"));
  m_keyEdit->setEchoMode (QLineEdit::Password);
  m_addButton = new QPushButton (tr ("Add"), this);

  auto *formLayout = new QFormLayout ();
  formLayout->addRow (tr ("Vendor"), m_vendorBox);
  formLayout->addRow (tr ("Label"), m_labelEdit);
  formLayout->addRow (tr ("API Key"), m_keyEdit);
  rootLayout->addLayout (formLayout);

  auto *actionRow = new QHBoxLayout ();
  actionRow->addStretch (1);
  actionRow->addWidget (m_addButton);
  rootLayout->addLayout (actionRow);

  connect (m_model, &CodingPlanModel::providersChanged, this,
           &ManualAccountsWidget::refreshVendorBox);
  connect (m_model, &CodingPlanModel::snapshotsChanged, this,
           &ManualAccountsWidget::rebuildAccountRows);
  connect (m_addButton, &QPushButton::clicked, this,
           &ManualAccountsWidget::onAddClicked);
  connect (m_keyEdit, &QLineEdit::returnPressed, this,
           &ManualAccountsWidget::onAddClicked);

  refreshVendorBox ();
  rebuildAccountRows ();
}

void
ManualAccountsWidget::refreshVendorBox ()
{
  const QSignalBlocker blocker (m_vendorBox);
  m_vendorBox->clear ();
  const QVariantList capable = m_model->manualCapableProviders ();
  for (const QVariant &item : capable)
    {
      const QVariantMap provider = item.toMap ();
      m_vendorBox->addItem (provider.value (QStringLiteral ("name")).toString (),
                            provider.value (QStringLiteral ("id")).toString ());
    }
  if (m_vendorBox->count () > 0 && m_vendorBox->currentIndex () < 0)
    {
      m_vendorBox->setCurrentIndex (0);
    }
  m_addButton->setEnabled (m_vendorBox->count () > 0);
}

void
ManualAccountsWidget::rebuildAccountRows ()
{
  while (m_rowsLayout->count () > 0)
    {
      QLayoutItem *item = m_rowsLayout->takeAt (0);
      delete item->widget ();
      delete item;
    }

  const QVariantList snapshots = m_model->snapshots ();
  for (const QVariant &item : snapshots)
    {
      const QVariantMap snapshot = item.toMap ();
      if (snapshot.value (QStringLiteral ("source")).toString ()
          != QStringLiteral ("manual"))
        {
          continue;
        }
      const QString entryId = snapshot.value (QStringLiteral ("entryId")).toString ();

      auto *row = new QWidget (this);
      auto *rowLayout = new QHBoxLayout (row);
      rowLayout->setContentsMargins (0, 0, 0, 0);
      auto *nameLabel = new QLabel (planTitle (snapshot), row);
      auto *removeButton = new QPushButton (tr ("Remove"), row);
      rowLayout->addWidget (nameLabel, 1);
      rowLayout->addWidget (removeButton);
      connect (removeButton, &QPushButton::clicked, this, [this, entryId] () {
        m_model->removeAccount (entryId);
      });
      m_rowsLayout->addWidget (row);
    }
}

void
ManualAccountsWidget::onAddClicked ()
{
  const QString providerId = m_vendorBox->currentData ().toString ();
  const QString apiKey = m_keyEdit->text ().trimmed ();
  if (providerId.isEmpty () || apiKey.isEmpty ())
    {
      return;
    }
  m_model->addAccount (providerId, m_labelEdit->text ().trimmed (), apiKey);
  m_labelEdit->clear ();
  m_keyEdit->clear ();
}

QByteArray
CodingPlanSettings::buildSchema ()
{
  // The dialog only renders options that live in a child group of a top
  // group, so each group carries one unnamed subgroup (rendered as a bare
  // rounded row container). Option "key" fields are short names; DSettings
  // prefixes them with the group path, so the runtime keys are
  // "display.plans.list" and "accounts.manual.manage". Both options are
  // custom view types; no values are stored in DSettings itself.
  const QString json = QStringLiteral (R"({
    "groups": [
        {
            "key": "display",
            "name": "%1",
            "groups": [
                {
                    "key": "plans",
                    "name": "",
                    "options": [
                        {
                            "key": "list",
                            "name": "%1",
                            "type": "codingplan-planlist"
                        }
                    ]
                }
            ]
        },
        {
            "key": "accounts",
            "name": "%2",
            "groups": [
                {
                    "key": "manual",
                    "name": "",
                    "options": [
                        {
                            "key": "manage",
                            "name": "%2",
                            "type": "codingplan-accounts"
                        }
                    ]
                }
            ]
        }
    ]
})")
      .arg (tr ("Show Plans"), tr ("Add Plan"));
  return json.toUtf8 ();
}

CodingPlanSettings::CodingPlanSettings (CodingPlanModel *model, QObject *parent)
    : QObject (parent), m_model (model)
{
  m_settings = Dtk::Core::DSettings::fromJson (buildSchema ());
  m_settings->setParent (this);
}

CodingPlanSettings::~CodingPlanSettings () = default;

void
CodingPlanSettings::showDialog ()
{
  if (m_dialog)
    {
      m_dialog->raise ();
      m_dialog->activateWindow ();
      return;
    }

  auto *dialog = new Dtk::Widget::DSettingsDialog;
  dialog->setAttribute (Qt::WA_DeleteOnClose);
  dialog->widgetFactory ()->registerWidget (
      QString::fromLatin1 (kPlanListViewType),
      [this] (QObject *) -> QWidget * {
        return new PlanVisibilityWidget (m_model);
      });
  dialog->widgetFactory ()->registerWidget (
      QString::fromLatin1 (kAccountsViewType),
      [this] (QObject *) -> QWidget * {
        return new ManualAccountsWidget (m_model);
      });
  // Group/option names in the schema are translated by the dialog through
  // qApp->translate(context, ...); point it at our class context so the
  // names resolve against this plugin's translation catalog.
  dialog->updateSettings (QByteArrayLiteral ("CodingPlanSettings"), m_settings);
  // Both options are custom widgets over the model; there is nothing DTK
  // could reset to a default. Must run after updateSettings(): the button is
  // created there, and setResetVisible silently ignores a missing child.
  dialog->setResetVisible (false);
  m_dialog = dialog;
  connect (dialog, &QObject::destroyed, this, [this] () { m_dialog.clear (); });
  dialog->open ();
}
