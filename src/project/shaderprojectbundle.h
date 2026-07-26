#ifndef SHADERPROJECTBUNDLE_H
#define SHADERPROJECTBUNDLE_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QJsonObject>

class ShaderProjectBundle
{
public:
    struct ExtractResult {
        bool ok = false;
        QString tempDir;
        QJsonObject projectJson;
        QString errorMessage;
    };

    static ExtractResult extractToTemp(const QString &stprojPath);
    static bool createBundle(const QString &outputPath, const QString &projectJsonPath,
                             const QStringList &assetPaths, QString *errorOut = nullptr);
    static void cleanupTempDir(const QString &tempDir);

    static bool isBundle(const QString &path);
};

#endif
