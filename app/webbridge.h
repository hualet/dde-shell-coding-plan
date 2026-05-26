// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "codingplanmodel.h"

#include <QObject>
#include <QVariantList>

class WebBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList providers READ providers NOTIFY dataChanged)
    Q_PROPERTY(QVariantList snapshots READ snapshots NOTIFY dataChanged)

public:
    explicit WebBridge(QObject *parent = nullptr);

    QVariantList providers() const;
    QVariantList snapshots() const;

    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void refreshProvider(const QString &providerId);
    Q_INVOKABLE void openConsole(const QString &providerId);
    Q_INVOKABLE void requestLogin(const QString &providerId);
    Q_INVOKABLE void finishLogin(const QString &providerId);
    Q_INVOKABLE void setManualRatio(const QString &providerId, double ratio);
    Q_INVOKABLE void setWebViewResult(const QString &providerId, const QVariantMap &result);
    Q_INVOKABLE QVariantMap getProviderConfig(const QString &providerId) const;

signals:
    void dataChanged();
    void loginPageRequested(const QString &providerId, const QString &loginUrl);
    void loginFinished(const QString &providerId);

private:
    CodingPlanModel *m_model = nullptr;
};
