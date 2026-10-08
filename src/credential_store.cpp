// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "credential_store.h"
#include "quota_parsers.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace
{
QJsonObject
loadJsonObject (const QString &path)
{
  QFile file (path);
  if (!file.open (QIODevice::ReadOnly))
    {
      return {};
    }
  const QJsonDocument document = QJsonDocument::fromJson (file.readAll ());
  return document.object ();
}

QString
nonEmpty (const QString &value)
{
  const QString trimmed = value.trimmed ();
  return trimmed.isEmpty () ? QString () : trimmed;
}
}

QString
CredentialStore::codexAuthPath ()
{
  const QString codexHome = qEnvironmentVariable ("CODEX_HOME");
  if (!codexHome.isEmpty ())
    {
      return QDir (codexHome).filePath (QStringLiteral ("auth.json"));
    }
  return QDir::home ().filePath (QStringLiteral (".codex/auth.json"));
}

QString
CredentialStore::kimiCredentialsPath ()
{
  const QString kimiHome = qEnvironmentVariable ("KIMI_CODE_HOME");
  const QString base = kimiHome.isEmpty ()
      ? QDir::home ().filePath (QStringLiteral (".kimi-code"))
      : kimiHome;
  return QDir (base).filePath (QStringLiteral ("credentials/kimi-code.json"));
}

QString
CredentialStore::zcodeConfigPath ()
{
  return QDir::home ().filePath (QStringLiteral (".zcode/v2/config.json"));
}

QString
CredentialStore::zcodeSettingPath ()
{
  return QDir::home ().filePath (QStringLiteral (".zcode/v2/setting.json"));
}

CodexCredentials
CredentialStore::readCodexCredentials (const QString &path)
{
  CodexCredentials result;
  const QJsonObject root = loadJsonObject (path);
  const QJsonObject tokens = root.value (QStringLiteral ("tokens")).toObject ();
  result.accessToken = nonEmpty (tokens.value (QStringLiteral ("access_token")).toString ());
  result.refreshToken = nonEmpty (tokens.value (QStringLiteral ("refresh_token")).toString ());
  result.idToken = nonEmpty (tokens.value (QStringLiteral ("id_token")).toString ());
  result.accountId = nonEmpty (tokens.value (QStringLiteral ("account_id")).toString ());
  result.authMode = nonEmpty (root.value (QStringLiteral ("auth_mode")).toString ());
  result.apiKeyMode = result.authMode.compare (QStringLiteral ("apikey"),
                                               Qt::CaseInsensitive)
      == 0;
  result.present = !result.accessToken.isEmpty ();
  return result;
}

KimiCredentials
CredentialStore::readKimiCredentials (const QString &path)
{
  KimiCredentials result;
  const QJsonObject root = loadJsonObject (path);
  result.accessToken = nonEmpty (root.value (QStringLiteral ("access_token")).toString ());
  const double expiresAt = root.value (QStringLiteral ("expires_at")).toDouble (-1);
  if (expiresAt > 0)
    {
      // Kimi writes seconds; stay tolerant of a millisecond writer.
      result.expiresAtMs = QuotaParsers::epochToDateTime (
          static_cast<qint64> (expiresAt)).toMSecsSinceEpoch ();
    }
  result.present = !result.accessToken.isEmpty ();
  return result;
}

GlmPlanCredentials
CredentialStore::readGlmPlanFromZCode (const QString &configPath,
                                       const QString &settingPath)
{
  GlmPlanCredentials result;
  const QJsonObject config = loadJsonObject (configPath).value (
      QStringLiteral ("provider")).toObject ();
  if (config.isEmpty ())
    {
      return result;
    }

  auto providerKeyWithApiKey = [&config](const QString &providerKey) {
    if (providerKey.isEmpty ())
      {
        return QString ();
      }
    const QJsonObject entry = config.value (providerKey).toObject ();
    const QString apiKey = nonEmpty (
        entry.value (QStringLiteral ("options")).toObject ()
            .value (QStringLiteral ("apiKey")).toString ());
    return apiKey.isEmpty () ? QString () : providerKey;
  };

  // The active provider is named by setting.json: the family domain selects
  // an entry of modelProviderFamilySelectedKeys shaped
  // "coding-plan:builtin:bigmodel-coding-plan".
  QString selectedKey;
  if (!settingPath.isEmpty ())
    {
      const QJsonObject setting = loadJsonObject (settingPath);
      const QString domain = nonEmpty (
          setting.value (QStringLiteral ("providerFamilyDomain")).toString ());
      const QJsonObject selectedKeys = setting.value (
          QStringLiteral ("modelProviderFamilySelectedKeys")).toObject ();
      const QString selectedValue = nonEmpty (selectedKeys.value (domain).toString ());
      const int builtinIndex = selectedValue.indexOf (QStringLiteral ("builtin:"));
      if (builtinIndex >= 0)
        {
          selectedKey = providerKeyWithApiKey (
              selectedValue.mid (builtinIndex));
        }
    }

  // Fallback: any coding-plan provider that actually holds a key.
  if (selectedKey.isEmpty ())
    {
      const QStringList keys = config.keys ();
      for (const QString &key : keys)
        {
          if (!key.contains (QStringLiteral ("coding-plan")))
            {
              continue;
            }
          if (!providerKeyWithApiKey (key).isEmpty ())
            {
              selectedKey = key;
              break;
            }
        }
    }

  if (selectedKey.isEmpty ())
    {
      return result;
    }

  const QJsonObject entry = config.value (selectedKey).toObject ();
  result.apiKey = nonEmpty (entry.value (QStringLiteral ("options")).toObject ()
                                .value (QStringLiteral ("apiKey")).toString ());
  const QString baseUrl = nonEmpty (entry.value (QStringLiteral ("options")).toObject ()
                                        .value (QStringLiteral ("baseURL")).toString ());
  const QUrl url (baseUrl);
  if (!result.apiKey.isEmpty () && url.isValid () && !url.host ().isEmpty ())
    {
      result.rootUrl = QStringLiteral ("%1://%2").arg (url.scheme (), url.host ());
      result.present = true;
    }
  return result;
}
