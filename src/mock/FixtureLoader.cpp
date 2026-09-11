#include "mock/FixtureLoader.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{

bool parseNode(const QJsonObject& obj, SizeNode& parent, const QString& where, QString* error)
{
    const QJsonValue nameValue = obj.value(QLatin1String("name"));
    if (!nameValue.isString() || nameValue.toString().isEmpty())
    {
        *error = QStringLiteral("%1: \"name\" must be a non-empty string").arg(where);
        return false;
    }
    const QString name = nameValue.toString();
    const QString here = where + QLatin1Char('/') + name;

    const QJsonValue childrenValue = obj.value(QLatin1String("children"));
    if (!childrenValue.isUndefined())
    {
        if (!childrenValue.isArray())
        {
            *error = QStringLiteral("%1: \"children\" must be an array").arg(here);
            return false;
        }
        SizeNode* folder = parent.addFolder(name);
        for (const QJsonValue& child : childrenValue.toArray())
        {
            if (!child.isObject() || !parseNode(child.toObject(), *folder, here, error))
            {
                if (error->isEmpty())
                {
                    *error = QStringLiteral("%1: every child must be an object").arg(here);
                }
                return false;
            }
        }
        return true;
    }

    const QJsonValue sizeValue = obj.value(QLatin1String("size"));
    if (!sizeValue.isDouble() || sizeValue.toDouble() < 0)
    {
        *error = QStringLiteral("%1: a file needs a non-negative \"size\"").arg(here);
        return false;
    }

    qint64 mtime = 0;
    const QJsonValue modifiedValue = obj.value(QLatin1String("modified"));
    if (!modifiedValue.isUndefined())
    {
        const QDateTime dt = QDateTime::fromString(modifiedValue.toString(), Qt::ISODate);
        if (!dt.isValid())
        {
            *error = QStringLiteral("%1: \"modified\" must be an ISO 8601 date").arg(here);
            return false;
        }
        mtime = dt.toSecsSinceEpoch();
    }

    parent.addFile(name, static_cast<qint64>(sizeValue.toDouble()), mtime);
    return true;
}

} // namespace

std::unique_ptr<AccountSnapshot> parseFixture(const QByteArray& json, QString* error)
{
    QString ignored;
    QString& err = error ? *error : ignored;
    err.clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (doc.isNull())
    {
        err = QStringLiteral("invalid JSON at offset %1: %2")
                  .arg(parseError.offset)
                  .arg(parseError.errorString());
        return nullptr;
    }
    const QJsonObject top = doc.object();
    const QJsonValue roots = top.value(QLatin1String("roots"));
    if (!roots.isArray())
    {
        err = QStringLiteral("top level: \"roots\" must be an array");
        return nullptr;
    }

    auto snapshot = std::make_unique<AccountSnapshot>();
    snapshot->root = std::make_unique<SizeNode>();
    snapshot->root->kind = NodeKind::Folder;
    for (const QJsonValue& root : roots.toArray())
    {
        if (!root.isObject() || !root.toObject().contains(QLatin1String("children")))
        {
            err = QStringLiteral("roots: every root must be a folder object");
            return nullptr;
        }
        if (!parseNode(root.toObject(), *snapshot->root, QStringLiteral("roots"), &err))
        {
            return nullptr;
        }
    }
    finalizeTree(*snapshot->root);

    const QJsonObject usage = top.value(QLatin1String("usage")).toObject();
    snapshot->usage.used = static_cast<qint64>(
        usage.value(QLatin1String("used")).toDouble(static_cast<double>(snapshot->root->size)));
    snapshot->usage.total =
        static_cast<qint64>(usage.value(QLatin1String("total")).toDouble(-1));
    return snapshot;
}

std::unique_ptr<AccountSnapshot> loadFixtureFile(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        }
        return nullptr;
    }
    return parseFixture(file.readAll(), error);
}
