// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "system_proxy.h"

#include <QDateTime>
#include <QHostAddress>
#include <QMutexLocker>
#include <QProcess>
#include <QUrl>

namespace SystemProxy
{
namespace
{
constexpr qint64 kCacheTtlMs = 10 * 1000;
constexpr int kGSettingsTimeoutMs = 1000;

QString
unquoteGVariantString (const QString &raw)
{
  const QString trimmed = raw.trimmed ();
  if (trimmed.size () < 2)
    {
      return trimmed;
    }
  const QChar quote = trimmed.at (0);
  if ((quote != QLatin1Char ('\'') && quote != QLatin1Char ('"'))
      || trimmed.at (trimmed.size () - 1) != quote)
    {
      return trimmed;
    }
  QString out;
  out.reserve (trimmed.size () - 2);
  for (int i = 1; i < trimmed.size () - 1; ++i)
    {
      if (trimmed.at (i) == QLatin1Char ('\\') && i + 1 < trimmed.size () - 1)
        {
          ++i;
        }
      out.append (trimmed.at (i));
    }
  return out;
}

QStringList
parseGVariantStringList (const QString &raw)
{
  QStringList items;
  const int open = raw.indexOf (QLatin1Char ('['));
  const int close = raw.lastIndexOf (QLatin1Char (']'));
  if (open < 0 || close <= open)
    {
      return items;
    }
  const QString inner = raw.mid (open + 1, close - open - 1);
  int i = 0;
  while (i < inner.size ())
    {
      while (i < inner.size ()
             && (inner.at (i).isSpace () || inner.at (i) == QLatin1Char (',')))
        {
          ++i;
        }
      if (i >= inner.size ())
        {
          break;
        }
      const QChar quote = inner.at (i);
      if (quote != QLatin1Char ('\'') && quote != QLatin1Char ('"'))
        {
          while (i < inner.size () && inner.at (i) != QLatin1Char (','))
            {
              ++i;
            }
          continue;
        }
      ++i;
      QString token;
      while (i < inner.size () && inner.at (i) != quote)
        {
          if (inner.at (i) == QLatin1Char ('\\') && i + 1 < inner.size ())
            {
              ++i;
            }
          token.append (inner.at (i));
          ++i;
        }
      if (i < inner.size () && inner.at (i) == quote)
        {
          ++i;
        }
      const QString trimmed = token.trimmed ();
      if (!trimmed.isEmpty ())
        {
          items.append (trimmed);
        }
    }
  return items;
}

quint16
parsePort (const QString &raw)
{
  bool ok = false;
  const int port = raw.trimmed ().toInt (&ok);
  return (ok && port > 0 && port <= 65535) ? static_cast<quint16> (port) : 0;
}

QString
stripBrackets (const QString &value)
{
  if (value.size () >= 2 && value.startsWith (QLatin1Char ('['))
      && value.endsWith (QLatin1Char (']')))
    {
      return value.mid (1, value.size () - 2);
    }
  return value;
}

bool
hasProxyEnvVars ()
{
  static const char *const kVars[] = {
    "http_proxy", "https_proxy", "all_proxy",
    "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY",
  };
  for (const char *name : kVars)
    {
      if (!qEnvironmentVariable (name).trimmed ().isEmpty ())
        {
          return true;
        }
    }
  return false;
}

bool
hasExplicitProxy (const QList<QNetworkProxy> &proxies)
{
  for (const QNetworkProxy &proxy : proxies)
    {
      if (proxy.type () != QNetworkProxy::NoProxy
          && proxy.type () != QNetworkProxy::DefaultProxy)
        {
          return true;
        }
    }
  return false;
}

QNetworkProxy
makeHttpProxy (const ProxyEndpoint &endpoint)
{
  QNetworkProxy proxy (QNetworkProxy::HttpProxy, endpoint.host, endpoint.port);
  if (endpoint.useAuth && !endpoint.user.isEmpty ())
    {
      proxy.setUser (endpoint.user);
      proxy.setPassword (endpoint.password);
    }
  return proxy;
}

QNetworkProxy
makeSocksProxy (const ProxyEndpoint &endpoint)
{
  return QNetworkProxy (QNetworkProxy::Socks5Proxy, endpoint.host, endpoint.port);
}
} // namespace

GSettingsProxyConfig
parseGSettingsOutput (const QString &output)
{
  GSettingsProxyConfig config;
  const QStringList lines = output.split (QLatin1Char ('\n'), Qt::SkipEmptyParts);
  for (const QString &rawLine : lines)
    {
      const QString line = rawLine.trimmed ();
      const int firstSpace = line.indexOf (QLatin1Char (' '));
      if (firstSpace <= 0)
        {
          continue;
        }
      const int secondSpace = line.indexOf (QLatin1Char (' '), firstSpace + 1);
      if (secondSpace <= firstSpace + 1)
        {
          continue;
        }

      const QString schema = line.left (firstSpace);
      const QString key = line.mid (firstSpace + 1, secondSpace - firstSpace - 1);
      const QString value = line.mid (secondSpace + 1).trimmed ();

      if (schema.endsWith (QStringLiteral (".system.proxy")))
        {
          if (key == QStringLiteral ("mode"))
            {
              config.mode = unquoteGVariantString (value).trimmed ().toLower ();
            }
          else if (key == QStringLiteral ("use-same-proxy"))
            {
              config.useSameProxy = (value == QStringLiteral ("true"));
            }
          else if (key == QStringLiteral ("ignore-hosts"))
            {
              config.ignoreHosts = parseGVariantStringList (value);
            }
        }
      else if (schema.endsWith (QStringLiteral (".system.proxy.http")))
        {
          // Note: org.gnome.system.proxy.http.enabled is a legacy GNOME 2 key
          // ignored by modern desktops and proxy tools (mode='manual' governs).
          if (key == QStringLiteral ("host"))
            {
              config.http.host = unquoteGVariantString (value).trimmed ();
            }
          else if (key == QStringLiteral ("port"))
            {
              config.http.port = parsePort (value);
            }
          else if (key == QStringLiteral ("use-authentication"))
            {
              config.http.useAuth = (value == QStringLiteral ("true"));
            }
          else if (key == QStringLiteral ("authentication-user"))
            {
              config.http.user = unquoteGVariantString (value);
            }
          else if (key == QStringLiteral ("authentication-password"))
            {
              config.http.password = unquoteGVariantString (value);
            }
        }
      else if (schema.endsWith (QStringLiteral (".system.proxy.https")))
        {
          if (key == QStringLiteral ("host"))
            {
              config.https.host = unquoteGVariantString (value).trimmed ();
            }
          else if (key == QStringLiteral ("port"))
            {
              config.https.port = parsePort (value);
            }
        }
      else if (schema.endsWith (QStringLiteral (".system.proxy.socks")))
        {
          if (key == QStringLiteral ("host"))
            {
              config.socks.host = unquoteGVariantString (value).trimmed ();
            }
          else if (key == QStringLiteral ("port"))
            {
              config.socks.port = parsePort (value);
            }
        }
    }
  return config;
}

bool
hostMatchesIgnoreList (const QString &host, const QStringList &ignoreHosts)
{
  const QString normalized = stripBrackets (host.trimmed ().toLower ());
  if (normalized.isEmpty ())
    {
      return false;
    }

  const QHostAddress hostAddr (normalized);
  for (const QString &rawPattern : ignoreHosts)
    {
      const QString pattern = stripBrackets (rawPattern.trimmed ().toLower ());
      if (pattern.isEmpty ())
        {
          continue;
        }
      if (pattern == QStringLiteral ("*"))
        {
          return true;
        }
      if (pattern == QStringLiteral ("<local>"))
        {
          if (!normalized.contains (QLatin1Char ('.'))
              && !normalized.contains (QLatin1Char (':')))
            {
              return true;
            }
          continue;
        }
      if (normalized == pattern)
        {
          return true;
        }
      if (pattern.startsWith (QStringLiteral ("*.")))
        {
          const QString suffix = pattern.mid (1);
          const QString bare = pattern.mid (2);
          if (normalized == bare || normalized.endsWith (suffix))
            {
              return true;
            }
          continue;
        }
      if (pattern.startsWith (QLatin1Char ('.')))
        {
          const QString bare = pattern.mid (1);
          if (normalized == bare || normalized.endsWith (pattern))
            {
              return true;
            }
          continue;
        }
      if (!hostAddr.isNull ())
        {
          if (pattern.contains (QLatin1Char ('/')))
            {
              const QPair<QHostAddress, int> subnet = QHostAddress::parseSubnet (pattern);
              if (!subnet.first.isNull () && hostAddr.isInSubnet (subnet))
                {
                  return true;
                }
            }
          else
            {
              const QHostAddress patternAddr (pattern);
              if (!patternAddr.isNull () && hostAddr == patternAddr)
                {
                  return true;
                }
            }
        }
    }
  return false;
}

QList<QNetworkProxy>
resolveFromConfig (const QNetworkProxyQuery &query,
                   const GSettingsProxyConfig &config)
{
  if (config.mode != QStringLiteral ("manual"))
    {
      return { QNetworkProxy::NoProxy };
    }

  QString host = query.peerHostName ().trimmed ();
  if (host.isEmpty ())
    {
      host = query.url ().host ().trimmed ();
    }
  if (!host.isEmpty () && hostMatchesIgnoreList (host, config.ignoreHosts))
    {
      return { QNetworkProxy::NoProxy };
    }

  QString scheme = query.protocolTag ().trimmed ().toLower ();
  if (scheme.isEmpty ())
    {
      scheme = query.url ().scheme ().trimmed ().toLower ();
    }

  if (config.useSameProxy && config.http.isValid ())
    {
      return { makeHttpProxy (config.http) };
    }

  if (scheme == QStringLiteral ("https"))
    {
      if (config.https.isValid ())
        {
          return { makeHttpProxy (config.https) };
        }
      if (config.socks.isValid ())
        {
          return { makeSocksProxy (config.socks) };
        }
      return { QNetworkProxy::NoProxy };
    }

  if (scheme == QStringLiteral ("http"))
    {
      if (config.http.isValid ())
        {
          return { makeHttpProxy (config.http) };
        }
      if (config.socks.isValid ())
        {
          return { makeSocksProxy (config.socks) };
        }
      return { QNetworkProxy::NoProxy };
    }

  if (config.socks.isValid ())
    {
      return { makeSocksProxy (config.socks) };
    }
  return { QNetworkProxy::NoProxy };
}

GSettingsProxyConfig
readGSettingsProxy ()
{
  static const QStringList schemas = {
    QStringLiteral ("org.gnome.system.proxy"),
    QStringLiteral ("com.deepin.wrap.gnome.system.proxy"),
  };

  for (const QString &schema : schemas)
    {
      QProcess process;
      process.start (QStringLiteral ("gsettings"),
                     { QStringLiteral ("list-recursively"), schema });
      if (!process.waitForFinished (kGSettingsTimeoutMs))
        {
          process.kill ();
          process.waitForFinished (200);
          continue;
        }
      if (process.exitStatus () == QProcess::NormalExit
          && process.exitCode () == 0)
        {
          const QString output = QString::fromUtf8 (
              process.readAllStandardOutput ());
          if (!output.trimmed ().isEmpty ())
            {
              return parseGSettingsOutput (output);
            }
        }
    }
  return {};
}

FallbackProxyFactory::FallbackProxyFactory (ConfigLoader loader)
    : m_loader (loader ? std::move (loader) : ConfigLoader (&readGSettingsProxy))
{
}

QList<QNetworkProxy>
FallbackProxyFactory::queryProxy (const QNetworkProxyQuery &query)
{
  const QList<QNetworkProxy> systemProxies
      = QNetworkProxyFactory::systemProxyForQuery (query);
  if (hasExplicitProxy (systemProxies) || hasProxyEnvVars ())
    {
      return systemProxies;
    }
  return resolveFromConfig (query, cachedConfig ());
}

GSettingsProxyConfig
FallbackProxyFactory::cachedConfig ()
{
  QMutexLocker locker (&m_mutex);
  const qint64 now = QDateTime::currentMSecsSinceEpoch ();
  if (m_cachedAtMs < 0 || now - m_cachedAtMs >= kCacheTtlMs)
    {
      m_cached = m_loader ();
      m_cachedAtMs = now;
    }
  return m_cached;
}
} // namespace SystemProxy
