// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providerregistry.h"

#include <QFile>
#include <QTest>

class ProviderRegistryTest : public QObject
{
  Q_OBJECT

private slots:
  void builtInWebViewProvidersCoverMvpPlatforms ();
  void snapshotStatusMapsToPanelSeverity ();
  void panelQmlUsesClassicTaskbarOrderAndDirectLoginCenter ();
};

void
ProviderRegistryTest::builtInWebViewProvidersCoverMvpPlatforms ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();

  const QStringList providerIds = registry.providerIds ();
  QCOMPARE (providerIds.size (), 3);
  QVERIFY (providerIds.contains (QStringLiteral ("codex")));
  QVERIFY (providerIds.contains (QStringLiteral ("kimi-code")));
  QVERIFY (providerIds.contains (QStringLiteral ("glm-coding")));

  for (const QString &providerId : providerIds)
    {
      const ProviderDefinition provider = registry.provider (providerId);
      QCOMPARE (provider.sourceType, SourceType::WebView);
      QVERIFY (!provider.loginUrl.isEmpty ());
      QVERIFY (!provider.quotaUrl.isEmpty ());
      QVERIFY (!provider.consoleUrl.isEmpty ());
      QVERIFY (!provider.allowedOrigins.isEmpty ());
    }
}

void
ProviderRegistryTest::snapshotStatusMapsToPanelSeverity ()
{
  QuotaSnapshot snapshot;
  snapshot.status = SnapshotStatus::Ok;
  snapshot.remainingRatio = 0.31;
  QCOMPARE (snapshot.severity (), PanelSeverity::Normal);

  snapshot.remainingRatio = 0.3;
  QCOMPARE (snapshot.severity (), PanelSeverity::Warning);

  snapshot.remainingRatio = 0.09;
  QCOMPARE (snapshot.severity (), PanelSeverity::Critical);

  snapshot.status = SnapshotStatus::ParseError;
  snapshot.remainingRatio = 0.9;
  QCOMPARE (snapshot.severity (), PanelSeverity::Error);
}

void
ProviderRegistryTest::panelQmlUsesClassicTaskbarOrderAndDirectLoginCenter ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (
      QStringLiteral ("useClassicTaskbarLayout: Panel.itemAlignment === Dock.LeftAlignment")));
  QVERIFY (qml.contains (
      QStringLiteral ("dockOrder: useClassicTaskbarLayout ? 21 : 10")));
  QVERIFY (qml.contains (QStringLiteral ("function openLoginCenter")));
  QVERIFY (qml.contains (QStringLiteral ("onClicked: root.openLoginCenter()")));
}

QTEST_MAIN (ProviderRegistryTest)

#include "providerregistry_test.moc"
