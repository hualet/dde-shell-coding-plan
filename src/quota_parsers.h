// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "providerregistry.h"

#include <QJsonObject>
#include <QString>

// Pure JSON -> QuotaSnapshot parsers for the four vendors' official usage
// endpoints. Each parser fills a copy of the given template snapshot (which
// already carries providerId/name/consoleUrl/source) and sets ratios, texts,
// status, message and resetAt. On failure the returned snapshot carries an
// error status (ParseError, or AuthError when the vendor says the key or
// session is bad) with a user-facing message; the caller turns error
// statuses into refreshFailed.

namespace QuotaParsers
{
constexpr int kSessionWindowMinutes = 300;   // 5h rolling window
constexpr int kWeeklyWindowMinutes = 10080;  // 7d window

double usedPercentToRemainingRatio (double usedPercent);
QDateTime epochToDateTime (qint64 value);
qint64 jwtExpirySeconds (const QString &jwt);

QuotaSnapshot parseCodexUsage (const QJsonObject &root,
                               const QuotaSnapshot &snapshot);
QuotaSnapshot parseKimiUsages (const QJsonObject &root,
                               const QuotaSnapshot &snapshot);
QuotaSnapshot parseGlmQuotaLimit (const QJsonObject &root,
                                  const QuotaSnapshot &snapshot);
QuotaSnapshot parseMinimaxRemains (const QJsonObject &root,
                                   const QuotaSnapshot &snapshot);
}
