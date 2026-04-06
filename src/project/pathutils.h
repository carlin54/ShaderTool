#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>

namespace PathUtils {
// If `relative` escapes the project directory (e.g. ../), returns empty.
QString safeResolveUnderProject(const QString &projectFilePath, const QString &relativePath);
} // namespace PathUtils

#endif
