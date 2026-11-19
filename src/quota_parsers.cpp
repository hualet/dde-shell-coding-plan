// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "quota_parsers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <cmath>

namespace QuotaParsers
{
// Free functions have no QObject base; route user-facing messages through
// this shim so they land in the "QuotaParsers" translation context.
struct ParserTr
{
  Q_DECLARE_TR_FUNCTIONS (QuotaParsers)
};
} // namespace QuotaParsers

namespace
{
struct Window
{
  double usedPercent = -1.0; // 0..100, -1 unknown
  double used = -1.0;        // raw count when the vendor tells one
  double total = -1.0;
  QDateTime resetAt;
  bool valid = false;
};

double
clampPercent (double value)
{
  if (std::isnan (value))
    {
      return -1.0;
    }
  return std::max (0.0, std::min (100.0, value));
}

// True when a window of this length is the weekly one rather than the
// 5-hour session.
bool
closerToWeekly (double minutes)
{
  return std::abs (minutes - QuotaParsers::kWeeklyWindowMinutes)
      < std::abs (minutes - QuotaParsers::kSessionWindowMinutes);
}

// Kimi sends numbers as strings or numbers, used or remaining.
double
numberOrNaN (const QJsonValue &value)
{
  if (value.isDouble ())
    {
      return value.toDouble ();
    }
  if (value.isString ())
    {
      bool ok = false;
      const double parsed = value.toString ().trimmed ().toDouble (&ok);
      return ok ? parsed : std::nan ("");
    }
  return std::nan ("");
}

void
applyWindow (QuotaSnapshot &snapshot, const Window &window, bool fiveHour)
{
  if (!window.valid)
    {
      return;
    }
  const double ratio = QuotaParsers::usedPercentToRemainingRatio (window.usedPercent);
  if (fiveHour)
    {
      snapshot.fiveHourRemainingRatio = ratio;
      snapshot.fiveHourBalanceText = QStringLiteral ("%1%").arg (
          qRound (ratio * 100));
    }
  else
    {
      snapshot.remainingRatio = ratio;
      snapshot.balanceText = QStringLiteral ("%1%").arg (
          qRound (ratio * 100));
      if (window.used >= 0 && window.total > 0)
        {
          snapshot.used = window.used;
          snapshot.total = window.total;
        }
    }
  // The 5-hour reset is the one users act on; it wins over the weekly one.
  if (!window.resetAt.isNull () && (fiveHour || snapshot.resetAt.isNull ()))
    {
      snapshot.resetAt = window.resetAt;
    }
}

// Kimi detail rows: {limit, used|remaining, resetTime|resetAt|...}.
Window
kimiDetailWindow (const QJsonObject &detail)
{
  Window window;
  const double limit = numberOrNaN (detail.value (QStringLiteral ("limit")));
  if (std::isnan (limit) || limit <= 0)
    {
      return window;
    }
  double used = numberOrNaN (detail.value (QStringLiteral ("used")));
  if (std::isnan (used))
    {
      const double remaining = numberOrNaN (
          detail.value (QStringLiteral ("remaining")));
      if (std::isnan (remaining))
        {
          return window;
        }
      used = limit - remaining;
    }
  window.total = limit;
  window.used = used;
  window.usedPercent = clampPercent (used / limit * 100);
  for (const QString &key : { QStringLiteral ("resetTime"),
                              QStringLiteral ("resetAt"),
                              QStringLiteral ("reset_at"),
                              QStringLiteral ("reset_time") })
    {
      const QString value = detail.value (key).toString ();
      if (!value.isEmpty ())
        {
          const QDateTime parsed = QDateTime::fromString (value, Qt::ISODateWithMs);
          if (parsed.isValid ())
            {
              window.resetAt = parsed;
              break;
            }
        }
    }
  window.valid = true;
  return window;
}
}

double
QuotaParsers::usedPercentToRemainingRatio (double usedPercent)
{
  if (std::isnan (usedPercent) || usedPercent < 0)
    {
      return -1.0; // unknown
    }
  const double percent = std::min (usedPercent, 100.0);
  return std::max (0.0, std::min (1.0, 1.0 - percent / 100.0));
}

QDateTime
QuotaParsers::epochToDateTime (qint64 value)
{
  if (value <= 0)
    {
      return {};
    }
  // Epochs are seconds when smaller than 1e10 (Sep 2286), milliseconds above.
  const qint64 ms = value > static_cast<qint64> (1e10) ? value : value * 1000;
  return QDateTime::fromMSecsSinceEpoch (ms, QTimeZone::UTC);
}

qint64
QuotaParsers::jwtExpirySeconds (const QString &jwt)
{
  const QStringList parts = jwt.split (QLatin1Char ('.'));
  if (parts.size () != 3)
    {
      return 0;
    }
  const QByteArray payload = QByteArray::fromBase64 (
      parts.at (1).toLatin1 (), QByteArray::Base64UrlEncoding);
  const QJsonDocument document = QJsonDocument::fromJson (payload);
  const double exp = document.object ().value (QStringLiteral ("exp")).toDouble (-1);
  return exp > 0 ? static_cast<qint64> (exp) : 0;
}

QuotaSnapshot
QuotaParsers::parseCodexUsage (const QJsonObject &root,
                               const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;
  const QJsonObject rateLimit = root.value (QStringLiteral ("rate_limit")).toObject ();

  const auto parseWindow = [](const QJsonObject &object) {
    Window window;
    const double usedPercent = object.value (QStringLiteral ("used_percent")).toDouble (-1);
    if (usedPercent < 0)
      {
        return window;
      }
    window.usedPercent = clampPercent (usedPercent);
    window.resetAt = epochToDateTime (
        static_cast<qint64> (object.value (QStringLiteral ("reset_at")).toDouble (0)));
    window.valid = true;
    return window;
  };

  const Window primary = parseWindow (
      rateLimit.value (QStringLiteral ("primary_window")).toObject ());
  const Window secondary = parseWindow (
      rateLimit.value (QStringLiteral ("secondary_window")).toObject ());
  if (!primary.valid && !secondary.valid)
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No quota windows in the Codex response.");
      return result;
    }

  // Classify by window length when told, else primary=5h / secondary=week.
  const auto minutes = [](const QJsonObject &object) {
    const double seconds = object.value (QStringLiteral ("limit_window_seconds")).toDouble (0);
    return seconds > 0 ? seconds / 60.0 : -1.0;
  };
  const double primaryMinutes = minutes (
      rateLimit.value (QStringLiteral ("primary_window")).toObject ());
  const double secondaryMinutes = minutes (
      rateLimit.value (QStringLiteral ("secondary_window")).toObject ());
  // primaryIsWeekly swaps the roles: primary → weekly, secondary → 5h.
  bool primaryIsWeekly = false;
  if (primary.valid && secondary.valid)
    {
      primaryIsWeekly = primaryMinutes > 0 && secondaryMinutes > 0
          && primaryMinutes > secondaryMinutes;
    }
  else if (primary.valid)
    {
      // A lone window (e.g. a free plan's weekly limit) goes by its length.
      primaryIsWeekly = primaryMinutes > 0 && closerToWeekly (primaryMinutes);
    }
  else
    {
      primaryIsWeekly = secondaryMinutes > 0 && !closerToWeekly (secondaryMinutes);
    }

  applyWindow (result, primaryIsWeekly ? secondary : primary, true);
  applyWindow (result, primaryIsWeekly ? primary : secondary, false);

  QString message = root.value (QStringLiteral ("plan_type")).toString ();
  const QJsonObject credits = root.value (QStringLiteral ("credits")).toObject ();
  if (credits.value (QStringLiteral ("has_credits")).toBool (false)
      && !credits.value (QStringLiteral ("unlimited")).toBool (true))
    {
      const QString balance = credits.value (QStringLiteral ("balance")).toString ().trimmed ();
      if (!balance.isEmpty ())
        {
          message = message.isEmpty ()
              ? QStringLiteral ("%1 credits").arg (balance)
              : QStringLiteral ("%1 · %2 credits").arg (message, balance);
        }
    }
  result.message = message;
  result.status = SnapshotStatus::Ok;
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}

QuotaSnapshot
QuotaParsers::parseClaudeUsage (const QJsonObject &root,
                                const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;

  // {"five_hour": {"utilization": 13.0, "resets_at": "...+00:00"},
  //  "seven_day": {...}, ...}; a window is null when it has not started.
  const auto parseWindow = [](const QJsonObject &object) {
    Window window;
    const double utilization = numberOrNaN (
        object.value (QStringLiteral ("utilization")));
    if (std::isnan (utilization))
      {
        return window;
      }
    window.usedPercent = clampPercent (utilization);
    window.resetAt = QDateTime::fromString (
        object.value (QStringLiteral ("resets_at")).toString (), Qt::ISODateWithMs);
    window.valid = true;
    return window;
  };

  const Window session = parseWindow (
      root.value (QStringLiteral ("five_hour")).toObject ());
  const Window weekly = parseWindow (
      root.value (QStringLiteral ("seven_day")).toObject ());
  if (!session.valid && !weekly.valid)
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No quota windows in the Claude response.");
      return result;
    }

  applyWindow (result, session, true);
  applyWindow (result, weekly, false);
  result.status = SnapshotStatus::Ok;
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}

QuotaSnapshot
QuotaParsers::parseClaudeDesktopHistory (const QJsonObject &root,
                                         const QuotaSnapshot &snapshot,
                                         const QDateTime &now)
{
  QuotaSnapshot result = snapshot;

  // {"version": 2, "samples": [{"t": <ms>, "org": "...", "u": {"fh": 13, "sd": 6}}]}
  // fh / sd are the five-hour and seven-day utilization percentages.
  const QJsonArray samples = root.value (QStringLiteral ("samples")).toArray ();
  QJsonObject latest;
  qint64 latestTime = 0;
  for (const QJsonValue &value : samples)
    {
      const QJsonObject sample = value.toObject ();
      const QJsonObject usage = sample.value (QStringLiteral ("u")).toObject ();
      const qint64 time = static_cast<qint64> (
          sample.value (QStringLiteral ("t")).toDouble (0));
      if ((usage.contains (QStringLiteral ("fh")) || usage.contains (QStringLiteral ("sd")))
          && time >= latestTime)
        {
          latest = usage;
          latestTime = time;
        }
    }
  if (latest.isEmpty ())
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("Claude Desktop has not recorded any quota yet.");
      return result;
    }

  const auto window = [&latest](const QString &key) {
    Window parsed;
    const double percent = numberOrNaN (latest.value (key));
    if (!std::isnan (percent))
      {
        parsed.usedPercent = clampPercent (percent);
        parsed.valid = true;
      }
    return parsed;
  };
  applyWindow (result, window (QStringLiteral ("fh")), true);
  applyWindow (result, window (QStringLiteral ("sd")), false);

  result.updatedAt = epochToDateTime (latestTime);
  result.status = SnapshotStatus::Ok;
  // Desktop samples every ~15 minutes while it runs; older data is suspect.
  if (result.updatedAt.isValid () && result.updatedAt.secsTo (now) > 60 * 60)
    {
      result.message = ParserTr::tr ("Recorded by Claude Desktop at %1, may be out of date")
                           .arg (result.updatedAt.toLocalTime ().toString (
                               QStringLiteral ("MM-dd HH:mm")));
    }
  else
    {
      result.message = ParserTr::tr ("From Claude Desktop");
    }
  return result;
}

QuotaSnapshot
QuotaParsers::parseKimiUsages (const QJsonObject &root,
                               const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;

  const Window weekly = kimiDetailWindow (
      root.value (QStringLiteral ("usage")).toObject ());
  applyWindow (result, weekly, false);

  Window session;
  double sessionDistance = -1.0;
  const QJsonArray limits = root.value (QStringLiteral ("limits")).toArray ();
  for (const QJsonValue &value : limits)
    {
      const QJsonObject limit = value.toObject ();
      const QJsonObject windowSpec = limit.value (QStringLiteral ("window")).toObject ();
      double minutes = numberOrNaN (windowSpec.value (QStringLiteral ("duration")));
      if (std::isnan (minutes))
        {
          continue;
        }
      const QString unit = windowSpec.value (QStringLiteral ("timeUnit")).toString ().toUpper ();
      if (unit.contains (QStringLiteral ("HOUR")))
        {
          minutes *= 60;
        }
      else if (unit.contains (QStringLiteral ("DAY")))
        {
          minutes *= 60 * 24;
        }
      else if (unit.contains (QStringLiteral ("SECOND")))
        {
          minutes /= 60;
        }

      QJsonObject detail = limit.value (QStringLiteral ("detail")).toObject ();
      if (detail.isEmpty ())
        {
          detail = windowSpec;
        }
      const Window candidate = kimiDetailWindow (detail);
      if (!candidate.valid)
        {
          continue;
        }
      const double distance = std::abs (minutes - kSessionWindowMinutes);
      if (!session.valid || (sessionDistance >= 0 && distance < sessionDistance))
        {
          session = candidate;
          sessionDistance = distance;
        }
    }
  applyWindow (result, session, true);

  if (!weekly.valid && !session.valid)
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No usable quota data in the Kimi response.");
      return result;
    }
  result.status = SnapshotStatus::Ok;
  result.unit = ParserTr::tr ("requests");
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}

QuotaSnapshot
QuotaParsers::parseGrokBilling (const QJsonObject &root,
                                const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;

  // {"config": {"creditUsagePercent": 26, "currentPeriod":
  //  {"type": "USAGE_PERIOD_TYPE_WEEKLY", "start": "...", "end": "..."}}}
  // Grok reports a single usage period (weekly for SuperGrok), so only the
  // weekly window fills; there is no 5-hour window to read.
  const QJsonObject config = root.value (QStringLiteral ("config")).toObject ();
  const double usedPercent = numberOrNaN (
      config.value (QStringLiteral ("creditUsagePercent")));
  if (std::isnan (usedPercent))
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No quota data in the Grok response.");
      return result;
    }

  Window weekly;
  weekly.usedPercent = clampPercent (usedPercent);
  const QJsonObject period = config.value (QStringLiteral ("currentPeriod")).toObject ();
  const QDateTime resetAt = QDateTime::fromString (
      period.value (QStringLiteral ("end")).toString (), Qt::ISODateWithMs);
  if (resetAt.isValid ())
    {
      weekly.resetAt = resetAt;
    }
  weekly.valid = true;
  applyWindow (result, weekly, false);

  result.status = SnapshotStatus::Ok;
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}

QuotaSnapshot
QuotaParsers::parseAntigravityUsage (const QJsonObject &root,
                                     const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;

  const auto looksLikeAuthError = [](const QString &text) {
    const QString lower = text.toLower ();
    return lower.contains (QStringLiteral ("sign in"))
        || lower.contains (QStringLiteral ("signed in"))
        || lower.contains (QStringLiteral ("login"))
        || lower.contains (QStringLiteral ("log in"))
        || lower.contains (QStringLiteral ("unauthenticated"))
        || lower.contains (QStringLiteral ("unauthorized"))
        || lower.contains (QStringLiteral ("credential"))
        || lower.contains (QStringLiteral ("not authenticated"));
  };

  const QString status = root.value (QStringLiteral ("status")).toString ().trimmed ();
  const QString response = root.value (QStringLiteral ("response")).toString ();
  if (!status.isEmpty ()
      && status.compare (QStringLiteral ("SUCCESS"), Qt::CaseInsensitive) != 0)
    {
      QString detail = root.value (QStringLiteral ("error")).toString ().trimmed ();
      if (detail.isEmpty ())
        {
          detail = root.value (QStringLiteral ("message")).toString ().trimmed ();
        }
      if (detail.isEmpty ())
        {
          detail = response.trimmed ();
        }
      if (looksLikeAuthError (detail))
        {
          result.status = SnapshotStatus::AuthError;
          result.message = ParserTr::tr ("Antigravity is not signed in; run agy once to sign in.");
          return result;
        }
      result.status = SnapshotStatus::ParseError;
      result.message = detail.isEmpty ()
          ? ParserTr::tr ("Antigravity CLI reported failure.")
          : detail;
      return result;
    }

  // `agy -p /usage --output-format json` returns structured quota buckets under
  // `command.data.groups[].buckets[]` (window: "5h" | "weekly",
  // remaining_fraction: 0..1, reset_time: ISO-8601) and a tab-separated table
  // in `response`. Prefer the structured groups and fall back to TSV parsing.
  // Several model pools can coexist (e.g. Gemini Models, Claude and GPT
  // models, Gemini 4 Argon); surface the tightest (lowest remaining / highest
  // used) pool for each window so warnings trigger as soon as any pool runs low.
  Window fiveHour;
  Window weekly;
  QString bottleneckGroup;
  double highestUsed = -1.0;

  const auto recordCandidate = [&](const QString &group, bool isFiveHour,
                                   bool isWeekly, const Window &candidate) {
    if (isFiveHour && (!fiveHour.valid || candidate.usedPercent > fiveHour.usedPercent))
      {
        fiveHour = candidate;
      }
    if (isWeekly && (!weekly.valid || candidate.usedPercent > weekly.usedPercent))
      {
        weekly = candidate;
      }
    if (!group.isEmpty () && candidate.usedPercent > highestUsed)
      {
        highestUsed = candidate.usedPercent;
        bottleneckGroup = group;
      }
  };

  const auto parseIsoTime = [](const QString &value) {
    if (value.isEmpty ())
      {
        return QDateTime ();
      }
    QDateTime parsed = QDateTime::fromString (value, Qt::ISODateWithMs);
    if (!parsed.isValid ())
      {
        parsed = QDateTime::fromString (value, Qt::ISODate);
      }
    return parsed;
  };

  const QJsonArray groups = root.value (QStringLiteral ("command")).toObject ()
                                .value (QStringLiteral ("data")).toObject ()
                                .value (QStringLiteral ("groups")).toArray ();
  for (const QJsonValue &groupVal : groups)
    {
      const QJsonObject groupObj = groupVal.toObject ();
      const QString groupName = groupObj.value (QStringLiteral ("name")).toString ().trimmed ();
      const QJsonArray buckets = groupObj.value (QStringLiteral ("buckets")).toArray ();
      for (const QJsonValue &bucketVal : buckets)
        {
          const QJsonObject bucket = bucketVal.toObject ();
          const QString windowTag = bucket.value (QStringLiteral ("window")).toString ().trimmed ().toLower ();
          const QString bucketName = bucket.value (QStringLiteral ("name")).toString ().trimmed ().toLower ();
          const bool isFiveHour = windowTag == QStringLiteral ("5h")
              || windowTag.contains (QStringLiteral ("five"))
              || bucketName.contains (QStringLiteral ("five hour"))
              || bucketName.contains (QStringLiteral ("5-hour"));
          const bool isWeekly = windowTag == QStringLiteral ("weekly")
              || windowTag == QStringLiteral ("7d")
              || bucketName.contains (QStringLiteral ("week"));
          if (!isFiveHour && !isWeekly)
            {
              continue;
            }

          const double fraction = numberOrNaN (
              bucket.value (QStringLiteral ("remaining_fraction")));
          if (std::isnan (fraction))
            {
              continue;
            }

          Window candidate;
          candidate.usedPercent = clampPercent ((1.0 - fraction) * 100.0);
          candidate.resetAt = parseIsoTime (
              bucket.value (QStringLiteral ("reset_time")).toString ().trimmed ());
          candidate.valid = true;
          recordCandidate (groupName, isFiveHour, isWeekly, candidate);
        }
    }

  if (!fiveHour.valid && !weekly.valid)
    {
      const QStringList lines = response.split (QLatin1Char ('\n'), Qt::SkipEmptyParts);
      for (const QString &rawLine : lines)
        {
          const QString line = rawLine.trimmed ();
          if (line.isEmpty ())
            {
              continue;
            }
          const QStringList cols = line.split (QLatin1Char ('\t'), Qt::SkipEmptyParts);
          if (cols.size () < 3)
            {
              continue;
            }

          const QString group = cols.at (0).trimmed ();
          const QString label = cols.at (1).trimmed ();
          const QString lowerLabel = label.toLower ();
          const bool isFiveHour = lowerLabel.contains (QStringLiteral ("five hour"))
              || lowerLabel.contains (QStringLiteral ("5 hour"))
              || lowerLabel.contains (QStringLiteral ("5-hour"))
              || lowerLabel.contains (QStringLiteral ("5h"))
              || lowerLabel.contains (QStringLiteral ("session"));
          const bool isWeekly = lowerLabel.contains (QStringLiteral ("week"))
              || lowerLabel.contains (QStringLiteral ("7 day"))
              || lowerLabel.contains (QStringLiteral ("7-day"))
              || lowerLabel.contains (QStringLiteral ("7d"));
          if (!isFiveHour && !isWeekly)
            {
              continue;
            }

          QString percentStr = cols.at (2).trimmed ();
          if (percentStr.endsWith (QLatin1Char ('%')))
            {
              percentStr.chop (1);
            }
          bool ok = false;
          const double pct = percentStr.trimmed ().toDouble (&ok);
          if (!ok || std::isnan (pct))
            {
              continue;
            }

          const bool isUsedMetric = lowerLabel.contains (QStringLiteral ("used"))
              && !lowerLabel.contains (QStringLiteral ("remaining"));
          const double usedPercent = clampPercent (isUsedMetric ? pct : (100.0 - pct));

          Window candidate;
          candidate.usedPercent = usedPercent;
          if (cols.size () >= 4)
            {
              candidate.resetAt = parseIsoTime (cols.at (3).trimmed ());
            }
          candidate.valid = true;
          recordCandidate (group, isFiveHour, isWeekly, candidate);
        }
    }

  if (!fiveHour.valid && !weekly.valid)
    {
      if (looksLikeAuthError (response))
        {
          result.status = SnapshotStatus::AuthError;
          result.message = ParserTr::tr ("Antigravity is not signed in; run agy once to sign in.");
          return result;
        }
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No quota data in the Antigravity response.");
      return result;
    }

  applyWindow (result, fiveHour, true);
  applyWindow (result, weekly, false);
  result.message = bottleneckGroup;
  result.status = SnapshotStatus::Ok;
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}

QuotaSnapshot
QuotaParsers::parseGlmQuotaLimit (const QJsonObject &root,
                                  const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;

  if (root.value (QStringLiteral ("success")).toBool (true) == false)
    {
      const QString msg = root.value (QStringLiteral ("msg")).toString ();
      const int code = root.value (QStringLiteral ("code")).toInt (0);
      // Zhipu answers 200 with success:false for a bad key.
      if (code == 401 || code == 403
          || msg.contains (QStringLiteral ("token"), Qt::CaseInsensitive)
          || msg.contains (QStringLiteral ("unauthorized"), Qt::CaseInsensitive))
        {
          result.status = SnapshotStatus::AuthError;
          result.message = ParserTr::tr ("The GLM API key is invalid or expired.");
          return result;
        }
      result.status = SnapshotStatus::ParseError;
      result.message = msg.isEmpty ()
          ? ParserTr::tr ("The GLM endpoint reported failure.")
          : QStringLiteral ("GLM：%1").arg (msg);
      return result;
    }

  const QJsonObject data = root.value (QStringLiteral ("data")).toObject ();
  const QJsonArray limits = data.value (QStringLiteral ("limits")).toArray ();

  struct Candidate
  {
    Window window;
    double minutes = -1.0;
    double count = -1.0;
    double total = -1.0;
  };
  QList<Candidate> candidates;
  for (const QJsonValue &value : limits)
    {
      const QJsonObject limit = value.toObject ();
      const QString type = limit.value (QStringLiteral ("type")).toString ();
      if (type.contains (QStringLiteral ("TIME_LIMIT")))
        {
          continue; // monthly MCP-call allowance, not a model window
        }

      static const QHash<int, int> unitMinutes = { { 1, 1440 }, { 3, 60 },
                                                   { 5, 1 }, { 6, 10080 } };
      const int unit = limit.value (QStringLiteral ("unit")).toInt (-1);
      int number = limit.value (QStringLiteral ("number")).toInt (0);
      if (!unitMinutes.contains (unit))
        {
          continue;
        }
      if (number <= 0)
        {
          number = 1; // a team's five hours come with no number
        }
      const double minutes = double (unitMinutes.value (unit)) * number;

      const double usage = numberOrNaN (limit.value (QStringLiteral ("usage")));
      const double currentValue = numberOrNaN (
          limit.value (QStringLiteral ("currentValue")));
      const double remaining = numberOrNaN (
          limit.value (QStringLiteral ("remaining")));
      const double percentage = numberOrNaN (
          limit.value (QStringLiteral ("percentage")));

      double usedPercent = std::nan ("");
      if (!std::isnan (usage) && usage > 0 && !std::isnan (currentValue))
        {
          usedPercent = currentValue / usage * 100;
        }
      else if (!std::isnan (usage) && usage > 0 && !std::isnan (remaining))
        {
          usedPercent = (usage - remaining) / usage * 100;
        }
      else
        {
          usedPercent = percentage;
        }
      if (std::isnan (usedPercent))
        {
          continue;
        }

      Candidate candidate;
      candidate.minutes = minutes;
      candidate.window.usedPercent = clampPercent (usedPercent);
      candidate.window.resetAt = epochToDateTime (
          static_cast<qint64> (limit.value (QStringLiteral ("nextResetTime")).toDouble (0)));
      candidate.window.valid = true;
      if (!std::isnan (usage) && usage > 0 && !std::isnan (currentValue))
        {
          candidate.count = currentValue;
          candidate.total = usage;
        }
      candidates.append (candidate);
    }

  const Candidate *session = nullptr;
  const Candidate *weekly = nullptr;
  double sessionDistance = -1.0;
  double weeklyDistance = -1.0;
  for (const Candidate &candidate : candidates)
    {
      const double sessionGap = std::abs (candidate.minutes - kSessionWindowMinutes);
      if (!session || sessionGap < sessionDistance)
        {
          session = &candidate;
          sessionDistance = sessionGap;
        }
      const double weeklyGap = std::abs (candidate.minutes - kWeeklyWindowMinutes);
      if (!weekly || weeklyGap < weeklyDistance)
        {
          weekly = &candidate;
          weeklyDistance = weeklyGap;
        }
    }

  if (!session && !weekly)
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No quota windows in the GLM response; the key may not be a coding plan key.");
      return result;
    }

  // One window can't be both rings: its length decides which it is.
  if (session == weekly)
    {
      if (closerToWeekly (session->minutes))
        {
          session = nullptr;
        }
      else
        {
          weekly = nullptr;
        }
    }
  if (session)
    {
      applyWindow (result, session->window, true);
    }
  if (weekly)
    {
      applyWindow (result, weekly->window, false);
      if (weekly->count >= 0 && weekly->total > 0)
        {
          result.used = weekly->count;
          result.total = weekly->total;
          result.unit = ParserTr::tr ("requests");
        }
    }

  const QString level = data.value (QStringLiteral ("level")).toString ().trimmed ();
  result.message = level.isEmpty () ? QString () : QStringLiteral ("GLM Coding %1").arg (level);
  result.status = SnapshotStatus::Ok;
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}

QuotaSnapshot
QuotaParsers::parseMinimaxRemains (const QJsonObject &root,
                                   const QuotaSnapshot &snapshot)
{
  QuotaSnapshot result = snapshot;

  const QJsonObject base = root.value (QStringLiteral ("base_resp")).toObject ();
  const int statusCode = base.value (QStringLiteral ("status_code")).toInt (-1);
  if (statusCode != 0)
    {
      const QString msg = base.value (QStringLiteral ("status_msg")).toString ();
      if (statusCode == 1004)
        {
          result.status = SnapshotStatus::AuthError;
          result.message = ParserTr::tr ("The MiniMax API key is invalid or expired.");
          return result;
        }
      result.status = SnapshotStatus::ParseError;
      result.message = msg.isEmpty ()
          ? ParserTr::tr ("The MiniMax endpoint returned an error (%1).").arg (statusCode)
          : QStringLiteral ("MiniMax：%1").arg (msg);
      return result;
    }

  const QJsonArray remains = root.value (QStringLiteral ("model_remains")).toArray ();
  QJsonObject bucket;
  for (const QJsonValue &value : remains)
    {
      const QJsonObject entry = value.toObject ();
      if (entry.value (QStringLiteral ("model_name")).toString ().toLower ()
          == QStringLiteral ("general"))
        {
          bucket = entry;
          break;
        }
    }
  if (bucket.isEmpty () && !remains.isEmpty ())
    {
      bucket = remains.at (0).toObject ();
    }
  if (bucket.isEmpty ())
    {
      result.status = SnapshotStatus::ParseError;
      result.message = ParserTr::tr ("No quota data in the MiniMax response.");
      return result;
    }

  // status 1 = metered, 2 = used up, 3 = unlimited. A metered window
  // without its percentage is unknown, never "100% left".
  bool unlimited = true;
  const auto windowFrom = [&bucket, &unlimited](const QString &statusKey,
                                                const QString &percentKey,
                                                const QString &endKey) {
    Window window;
    const int status = bucket.value (statusKey).toInt (1);
    if (status == 3)
      {
        return window; // unlimited: nothing to show
      }
    unlimited = false;
    if (status == 2)
      {
        window.usedPercent = 100.0;
      }
    else
      {
        const double leftPercent = numberOrNaN (bucket.value (percentKey));
        if (std::isnan (leftPercent))
          {
            return window;
          }
        window.usedPercent = clampPercent (100.0 - leftPercent);
      }
    const qint64 end = static_cast<qint64> (bucket.value (endKey).toDouble (0));
    if (end > 0)
      {
        window.resetAt = epochToDateTime (end);
      }
    window.valid = true;
    return window;
  };

  const Window session = windowFrom (
      QStringLiteral ("current_interval_status"),
      QStringLiteral ("current_interval_remaining_percent"),
      QStringLiteral ("end_time"));
  const Window weekly = windowFrom (
      QStringLiteral ("current_weekly_status"),
      QStringLiteral ("current_weekly_remaining_percent"),
      QStringLiteral ("weekly_end_time"));

  applyWindow (result, session, true);
  applyWindow (result, weekly, false);

  if (!session.valid && !weekly.valid)
    {
      if (!unlimited)
        {
          result.status = SnapshotStatus::ParseError;
          result.message = ParserTr::tr ("No recognizable quota percentages in the MiniMax response.");
          return result;
        }
      result.message = ParserTr::tr ("Unlimited");
      result.status = SnapshotStatus::Ok;
      result.updatedAt = QDateTime::currentDateTimeUtc ();
      return result;
    }
  result.status = SnapshotStatus::Ok;
  result.updatedAt = QDateTime::currentDateTimeUtc ();
  return result;
}
