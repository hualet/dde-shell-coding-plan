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
  void standaloneAppSourceFilesExist ();
  void reactFrontendEntryExists ();
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
  QVERIFY (qml.contains (QStringLiteral ("TapHandler")));
  QVERIFY (qml.contains (QStringLiteral ("acceptedButtons: Qt.LeftButton")));
  QVERIFY (qml.contains (QStringLiteral ("onTapped:")));
  QVERIFY (qml.contains (QStringLiteral ("root.openLoginCenter()")));
  QVERIFY (qml.contains (QStringLiteral ("webPopup.open()")));
  QVERIFY (!qml.contains (QStringLiteral ("webPopup.popupVisible = true")));
  QVERIFY (!qml.contains (QStringLiteral (
      "MouseArea {\n        anchors.fill: parent\n        onClicked: root.openLoginCenter()")));
}

void
ProviderRegistryTest::standaloneAppSourceFilesExist ()
{
  QFile mainCpp (QStringLiteral (SOURCE_DIR "/app/main.cpp"));
  QVERIFY2 (mainCpp.exists (), "app/main.cpp should exist");
  QVERIFY2 (mainCpp.open (QIODevice::ReadOnly), "app/main.cpp should be readable");

  QFile webbridge (QStringLiteral (SOURCE_DIR "/app/webbridge.h"));
  QVERIFY2 (webbridge.exists (), "app/webbridge.h should exist");

  const QString content = QString::fromUtf8 (mainCpp.readAll ());
  QVERIFY (content.contains (QStringLiteral ("QWebEngineView")));
  QVERIFY (content.contains (QStringLiteral ("QWebChannel")));
  QVERIFY (content.contains (QStringLiteral ("WebBridge")));
  QVERIFY (content.contains (QStringLiteral ("DMainWindow")));
}

void
ProviderRegistryTest::reactFrontendEntryExists ()
{
  QFile indexHtml (QStringLiteral (SOURCE_DIR "/web/index.html"));
  QVERIFY2 (indexHtml.exists (), "web/index.html should exist");

  QFile packageJson (QStringLiteral (SOURCE_DIR "/web/package.json"));
  QVERIFY2 (packageJson.open (QIODevice::ReadOnly), "web/package.json should be readable");

  const QString content = QString::fromUtf8 (packageJson.readAll ());
  QVERIFY (content.contains (QStringLiteral ("@mui/material")));
  QVERIFY (content.contains (QStringLiteral ("react")));
  QVERIFY (content.contains (QStringLiteral ("react-router-dom")));
}

QTEST_MAIN (ProviderRegistryTest)

#include "providerregistry_test.moc"
