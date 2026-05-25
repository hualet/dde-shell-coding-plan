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
  void panelQmlLeftClickOpensStatusPopupWithoutWebEngine ();
  void panelQmlRightClickOpensSettingsMenu ();
  void appletLaunchesStandaloneSettingsWindow ();
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
ProviderRegistryTest::panelQmlLeftClickOpensStatusPopupWithoutWebEngine ()
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
  QVERIFY (qml.contains (QStringLiteral ("popup.open()")));
  QVERIFY (qml.contains (QStringLiteral ("webPopup.open()")));
  QVERIFY (!qml.contains (QStringLiteral (
      "Panel.requestClosePopup()\n                root.openLoginCenter()")));
  QVERIFY (!qml.contains (QStringLiteral ("webPopup.popupVisible = true")));
  QVERIFY (!qml.contains (QStringLiteral (
      "MouseArea {\n        anchors.fill: parent\n        onClicked: root.openLoginCenter()")));

  const qsizetype popupStart = qml.indexOf (QStringLiteral ("id: popup"));
  const qsizetype webPopupStart = qml.indexOf (QStringLiteral ("id: webPopup"));
  QVERIFY (popupStart >= 0);
  QVERIFY (webPopupStart > popupStart);

  const QString statusPopup = qml.mid (popupStart, webPopupStart - popupStart);
  QVERIFY (statusPopup.contains (QStringLiteral ("5小时额度")));
  QVERIFY (statusPopup.contains (QStringLiteral ("周额度")));
  QVERIFY (statusPopup.contains (QStringLiteral ("anchors.centerIn: parent")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Refresh\")")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Login\")")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Console\")")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Manual 50%\")")));
}

void
ProviderRegistryTest::panelQmlRightClickOpensSettingsMenu ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (QStringLiteral ("import Qt.labs.platform 1.1 as LP")));
  QVERIFY (qml.contains (QStringLiteral ("acceptedButtons: Qt.RightButton")));
  QVERIFY (qml.contains (QStringLiteral ("settingsMenuLoader.item.open()")));
  QVERIFY (qml.contains (QStringLiteral ("text: qsTr(\"设置\")")));
  QVERIFY (qml.contains (QStringLiteral ("Applet.showSettings()")));
}

void
ProviderRegistryTest::appletLaunchesStandaloneSettingsWindow ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanapplet.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());
  QVERIFY (headerContent.contains (QStringLiteral ("Q_INVOKABLE")));
  QVERIFY (headerContent.contains (QStringLiteral ("showSettings")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanapplet.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());
  QVERIFY (sourceContent.contains (QStringLiteral ("QProcess::startDetached")));
  QVERIFY (sourceContent.contains (QStringLiteral ("/usr/bin/dde-coding-plan")));
  QVERIFY (sourceContent.contains (QStringLiteral ("dde-coding-plan")));
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
  QVERIFY (content.contains (QStringLiteral ("runJavaScript")));
  QVERIFY (content.contains (QStringLiteral ("allowedOrigins")));
  QVERIFY (content.contains (QStringLiteral ("setManualRatio")));
  QVERIFY (content.contains (QStringLiteral ("CODING_PLAN_WEB_DIR")));
  QVERIFY (content.contains (QStringLiteral ("/usr/share/dde-coding-plan/web")));
  QVERIFY (!content.contains (QStringLiteral ("qrc:/web/index.html")));
  QVERIFY (!content.contains (QStringLiteral ("loadTranslator()")));
}

void
ProviderRegistryTest::reactFrontendEntryExists ()
{
  QFile indexHtml (QStringLiteral (SOURCE_DIR "/web/index.html"));
  QVERIFY2 (indexHtml.open (QIODevice::ReadOnly), "web/index.html should be readable");
  const QString indexContent = QString::fromUtf8 (indexHtml.readAll ());
  QVERIFY (indexContent.contains (
      QStringLiteral ("qrc:///qtwebchannel/qwebchannel.js")));

  QFile packageJson (QStringLiteral (SOURCE_DIR "/web/package.json"));
  QVERIFY2 (packageJson.open (QIODevice::ReadOnly), "web/package.json should be readable");

  const QString content = QString::fromUtf8 (packageJson.readAll ());
  QVERIFY (content.contains (QStringLiteral ("@mui/material")));
  QVERIFY (content.contains (QStringLiteral ("react")));
  QVERIFY (content.contains (QStringLiteral ("react-router-dom")));

  QFile cmakeLists (QStringLiteral (SOURCE_DIR "/CMakeLists.txt"));
  QVERIFY2 (cmakeLists.open (QIODevice::ReadOnly), "CMakeLists.txt should be readable");

  const QString cmake = QString::fromUtf8 (cmakeLists.readAll ());
  QVERIFY (cmake.contains (QStringLiteral ("CODING_PLAN_WEB_DIR")));
  QVERIFY (cmake.contains (QStringLiteral ("install(")));
  QVERIFY (cmake.contains (QStringLiteral ("web/dist/")));
}

QTEST_MAIN (ProviderRegistryTest)

#include "providerregistry_test.moc"
