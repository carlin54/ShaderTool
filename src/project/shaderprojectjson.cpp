#include "shaderprojectjson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <cmath>

namespace {

bool validateValue(const QJsonValue &instance, const QJsonObject &schema, const QString &path, QString *err);

bool validateIntegerLike(const QJsonValue &instance, int minimum, const QString &path, QString *err)
{
    if (!instance.isDouble()) {
        if (err)
            *err = path + QStringLiteral(": expected integer.");
        return false;
    }
    const double d = instance.toDouble();
    if (std::floor(d) != d) {
        if (err)
            *err = path + QStringLiteral(": expected integer.");
        return false;
    }
    if (int(d) < minimum) {
        if (err)
            *err = path + QStringLiteral(": value below minimum %1.").arg(minimum);
        return false;
    }
    return true;
}

bool validateValue(const QJsonValue &instance, const QJsonObject &schema, const QString &path, QString *err)
{
    const QString type = schema.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("object")) {
        if (!instance.isObject()) {
            if (err)
                *err = path + QStringLiteral(": expected object.");
            return false;
        }
        const QJsonObject o = instance.toObject();

        const QJsonArray required = schema.value(QStringLiteral("required")).toArray();
        for (const QJsonValue &rv : required) {
            const QString k = rv.toString();
            if (!o.contains(k)) {
                if (err)
                    *err = path + QStringLiteral(": missing required property \"%1\".").arg(k);
                return false;
            }
        }

        const QJsonObject props = schema.value(QStringLiteral("properties")).toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            const QString k = it.key();
            if (!props.contains(k))
                continue;
            const QJsonObject sub = props.value(k).toObject();
            if (!validateValue(it.value(), sub, path + QLatin1Char('.') + k, err))
                return false;
        }
        return true;
    }

    if (type == QStringLiteral("array")) {
        if (!instance.isArray()) {
            if (err)
                *err = path + QStringLiteral(": expected array.");
            return false;
        }
        const QJsonArray arr = instance.toArray();
        const QJsonObject itemSchema = schema.value(QStringLiteral("items")).toObject();
        if (itemSchema.isEmpty())
            return true;
        for (int i = 0; i < arr.size(); ++i) {
            if (!validateValue(arr.at(i), itemSchema,
                               path + QStringLiteral("[%1]").arg(i), err))
                return false;
        }
        return true;
    }

    if (type == QStringLiteral("string")) {
        if (!instance.isString()) {
            if (err)
                *err = path + QStringLiteral(": expected string.");
            return false;
        }
        const QString s = instance.toString();
        if (schema.contains(QStringLiteral("const"))) {
            const QString c = schema.value(QStringLiteral("const")).toString();
            if (s != c) {
                if (err)
                    *err = path + QStringLiteral(": must be \"%1\".").arg(c);
                return false;
            }
        }
        if (schema.contains(QStringLiteral("enum"))) {
            const QJsonArray en = schema.value(QStringLiteral("enum")).toArray();
            bool ok = false;
            for (const QJsonValue &ev : en) {
                if (ev.toString() == s) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                if (err)
                    *err = path + QStringLiteral(": value not allowed.");
                return false;
            }
        }
        return true;
    }

    if (type == QStringLiteral("integer")) {
        const int minv = schema.value(QStringLiteral("minimum")).toInt(1);
        return validateIntegerLike(instance, minv, path, err);
    }

    if (type == QStringLiteral("number")) {
        if (!instance.isDouble()) {
            if (err)
                *err = path + QStringLiteral(": expected number.");
            return false;
        }
        return true;
    }

    if (err)
        *err = path + QStringLiteral(": unsupported schema type.");
    return false;
}

} // namespace

namespace ShaderProjectJson {

bool validateAgainstSchema(const QJsonObject &document, QString *errorOut)
{
    QFile f(QStringLiteral(":/schema/shaderproject.schema.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not open embedded schema :/schema/shaderproject.schema.json.");
        return false;
    }
    QJsonParseError pe{};
    const QJsonDocument sd = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !sd.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Embedded JSON schema is invalid.");
        return false;
    }
    return validateValue(QJsonValue(document), sd.object(), QStringLiteral("$"), errorOut);
}

} // namespace ShaderProjectJson
