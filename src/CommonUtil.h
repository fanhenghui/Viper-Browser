#ifndef COMMONUTIL_H
#define COMMONUTIL_H

#include <QIcon>
#include <QRegularExpression>
#include <QString>
#include <QtGlobal>

/// Collection of common utility functions
namespace CommonUtil
{
    /// Converts the given number of bytes into a user-readable string (ex: "10 MB", "1.2 GB" etc)
    QString bytesToUserFriendlyStr(quint64 amount);

    /// Converts the base64-encoded byte array into a QIcon
    QIcon iconFromBase64(QByteArray data);

    /// Returns the base64 encoding of the given icon
    QByteArray iconToBase64(QIcon icon);

    /// Computes and returns base^exp
    quint64 quPow(quint64 base, quint64 exp);

    /// Converts the match pattern into a QRegularExpression that can be applied to URLs
    /// See https://developer.chrome.com/extensions/match_patterns for match pattern specification
    QRegularExpression getRegExpForMatchPattern(const QString &str);
}

#endif // COMMONUTIL_H
