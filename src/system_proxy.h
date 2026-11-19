// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QMutex>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <functional>

namespace SystemProxy
{
struct ProxyEndpoint
{
  QString host;
  quint16 port = 0;
  bool useAuth = false;
  QString user;
  QString password;

  bool
  isValid () const
  {
    return !host.isEmpty () && port > 0;
  }
};

struct GSettingsProxyConfig
{
  QString mode; // "none", "manual", "auto"
  bool useSameProxy = false;
  QStringList ignoreHosts;
  ProxyEndpoint http;
  ProxyEndpoint https;
  ProxyEndpoint socks;
};

// Parses `gsettings list-recursively` output for org.gnome.system.proxy or
// com.deepin.wrap.gnome.system.proxy.
GSettingsProxyConfig parseGSettingsOutput (const QString &output);

// True when host matches a GNOME/Deepin ignore-hosts pattern (exact host,
// wildcard *.domain / .domain, <local>, or IP/CIDR subnet).
bool hostMatchesIgnoreList (const QString &host, const QStringList &ignoreHosts);

// Resolves a proxy from a parsed GSettingsProxyConfig for a given query.
QList<QNetworkProxy> resolveFromConfig (const QNetworkProxyQuery &query,
                                        const GSettingsProxyConfig &config);

// Reads the desktop's GSettings proxy configuration (tries
// org.gnome.system.proxy, then com.deepin.wrap.gnome.system.proxy).
GSettingsProxyConfig readGSettingsProxy ();

// Populates http_proxy/https_proxy/all_proxy/no_proxy on a child process
// environment from a parsed GSettingsProxyConfig, only when no proxy env var
// is already set.
void applyProxyEnvFromConfig (QProcessEnvironment &env,
                              const GSettingsProxyConfig &config);

// Reads GSettings (when no proxy env var is already set) and applies it to env
// so helper CLIs spawned from dde-shell honor the desktop system proxy.
void applyFallbackProxyEnv (QProcessEnvironment &env);

// Proxy factory for QNetworkAccessManager: delegates to Qt's system proxy
// first; when no proxy env var is set and Qt/libproxy returns only NoProxy
// (as on Deepin where XDG_CURRENT_DESKTOP=DDE makes libproxy's config-gnome
// skip org.gnome.system.proxy), falls back to reading GSettings directly.
class FallbackProxyFactory : public QNetworkProxyFactory
{
public:
  using ConfigLoader = std::function<GSettingsProxyConfig ()>;

  explicit FallbackProxyFactory (ConfigLoader loader = {});

  QList<QNetworkProxy>
  queryProxy (const QNetworkProxyQuery &query = QNetworkProxyQuery ()) override;

private:
  GSettingsProxyConfig cachedConfig ();

  ConfigLoader m_loader;
  QMutex m_mutex;
  GSettingsProxyConfig m_cached;
  qint64 m_cachedAtMs = -1;
};
} // namespace SystemProxy
