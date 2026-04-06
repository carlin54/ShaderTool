#ifndef SPIRVREFLECTUTIL_H
#define SPIRVREFLECTUTIL_H

#include <QByteArray>
#include <QString>

namespace SpirvReflectUtil {
// Human-readable descriptor bindings / push constants (empty if reflection fails).
QString describeBindings(const QByteArray &spirv);
} // namespace SpirvReflectUtil

#endif
