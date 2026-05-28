// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webbridge.h"

#include <QDebug>

WebBridge::WebBridge(QObject *parent)
    : QObject(parent)
{
    m_model = new CodingPlanModel(this);
    connect(m_model, &CodingPlanModel::snapshotsChanged, this, &WebBridge::dataChanged);
}

QVariantList WebBridge::providers() const
{
    return m_model->providers();
}

QVariantList WebBridge::snapshots() const
{
    return m_model->snapshots();
}

void WebBridge::refreshAll()
{
    emit refreshAllRequested();
}

void WebBridge::refreshProvider(const QString &providerId)
{
    QVariantMap provider = getProviderConfig(providerId);
    const QString quotaUrl = provider.value(QStringLiteral("quotaUrl")).toString();
    if (!quotaUrl.isEmpty()) {
        emit loginPageRequested(providerId, quotaUrl);
        return;
    }

    m_model->refreshProvider(providerId);
}

void WebBridge::openConsole(const QString &providerId)
{
    m_model->openConsole(providerId);
}

void WebBridge::requestLogin(const QString &providerId)
{
    QVariantList provs = m_model->providers();
    for (const QVariant &item : provs) {
        QVariantMap provider = item.toMap();
        if (provider.value(QStringLiteral("id")).toString() == providerId) {
            QString loginUrl = provider.value(QStringLiteral("loginUrl")).toString();
            emit loginPageRequested(providerId, loginUrl);
            return;
        }
    }
}

void WebBridge::finishLogin(const QString &providerId)
{
    emit loginFinished(providerId);
}

void WebBridge::setManualRatio(const QString &providerId, double ratio)
{
    m_model->setManualRatio(providerId, ratio);
}

void WebBridge::setWebViewResult(const QString &providerId, const QVariantMap &result)
{
    m_model->setWebViewResult(providerId, result);
}

void WebBridge::setProviderError(const QString &providerId, const QString &message)
{
    m_model->setProviderError(providerId, message);
}

QVariantMap WebBridge::getProviderConfig(const QString &providerId) const
{
    QVariantList provs = m_model->providers();
    for (const QVariant &item : provs) {
        QVariantMap provider = item.toMap();
        if (provider.value(QStringLiteral("id")).toString() == providerId) {
            return provider;
        }
    }
    return QVariantMap();
}
