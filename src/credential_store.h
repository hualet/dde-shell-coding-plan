// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

// Credentials are read from the local coding-agent CLI config files, the way
// orca and magpie do it. Everything is strictly read-only: the CLIs own
// their token lifecycle (the only exception is Codex, whose OAuth refresh
// is delegated to DirectQuotaProvider because the Codex CLI tolerates it).

struct CodexCredentials
{
  QString accessToken;
  QString refreshToken;
  QString idToken;
  QString accountId;
  QString authMode;
  bool present = false;   // auth.json exists and carries an access token
  bool apiKeyMode = false; // auth_mode == "apikey": no ChatGPT plan to read
};

struct KimiCredentials
{
  QString accessToken;
  qint64 expiresAtMs = 0;
  bool present = false;
};

struct GlmPlanCredentials
{
  QString apiKey;
  QString rootUrl; // e.g. "https://open.bigmodel.cn"
  bool present = false;
};

namespace CredentialStore
{
QString codexAuthPath ();
QString kimiCredentialsPath ();
QString zcodeConfigPath ();
QString zcodeSettingPath ();

CodexCredentials readCodexCredentials (const QString &path);
KimiCredentials readKimiCredentials (const QString &path);
GlmPlanCredentials readGlmPlanFromZCode (const QString &configPath,
                                         const QString &settingPath = QString ());
}
