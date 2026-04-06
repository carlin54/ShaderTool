#include "pathutils.h"

#include <QDir>
#include <QFileInfo>

QString PathUtils::safeResolveUnderProject(const QString &projectFilePath, const QString &relativePath)
{
    if (projectFilePath.isEmpty() || relativePath.isEmpty())
        return QString();

    if (QFileInfo(relativePath).isAbsolute())
        return QString();

    const QFileInfo projFi(projectFilePath);
    const QString base = projFi.absolutePath();
    const QString baseNorm = QDir::cleanPath(QDir(base).absolutePath());
    const QString candidateNorm = QDir::cleanPath(QDir(base).absoluteFilePath(relativePath));
    if (baseNorm.isEmpty() || candidateNorm.isEmpty())
        return QString();

    if (candidateNorm != baseNorm && !candidateNorm.startsWith(baseNorm + QDir::separator()))
        return QString();

    return candidateNorm;
}
