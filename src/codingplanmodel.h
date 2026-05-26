// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "providerregistry.h"

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QVariantList>

class CodingPlanModel : public QObject
{
  Q_OBJECT
  Q_PROPERTY (QVariantList providers READ providers CONSTANT)
  Q_PROPERTY (QVariantList snapshots READ snapshots NOTIFY snapshotsChanged)
  Q_PROPERTY (bool hasSubscriptions READ hasSubscriptions NOTIFY snapshotsChanged)
  Q_PROPERTY (QString tooltipText READ tooltipText NOTIFY snapshotsChanged)

public:
  explicit CodingPlanModel (QObject *parent = nullptr);

  QVariantList providers () const;
  QVariantList snapshots () const;
  bool hasSubscriptions () const;
  QString tooltipText () const;

  Q_INVOKABLE void refreshAll ();
  Q_INVOKABLE void refreshProvider (const QString &providerId);
  Q_INVOKABLE void openConsole (const QString &providerId);
  Q_INVOKABLE void openLogin (const QString &providerId);
  Q_INVOKABLE void clearSession (const QString &providerId);
  Q_INVOKABLE void setManualRatio (const QString &providerId, double ratio);
  Q_INVOKABLE void setWebViewResult (const QString &providerId,
                                       const QVariantMap &result);
  Q_INVOKABLE void setProviderError (const QString &providerId,
                                      const QString &message);
  Q_INVOKABLE void setProviderAuthenticated (const QString &providerId);

  void watchExternalChanges ();

signals:
  void snapshotsChanged ();
  void sessionCleared (const QString &providerId);

private:
  QuotaSnapshot createInitialSnapshot (const ProviderDefinition &provider) const;
  void loadSnapshots ();
  void saveSnapshots () const;
  int snapshotIndex (const QString &providerId) const;

  ProviderRegistry m_registry;
  QList<QuotaSnapshot> m_snapshots;
  QTimer m_refreshTimer;
  QFileSystemWatcher *m_settingsWatcher = nullptr;
  QTimer m_debounceTimer;
};
