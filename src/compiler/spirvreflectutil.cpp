#include "spirvreflectutil.h"

#include <spirv_reflect.h>

#include <QVector>
#include <QStringList>

QString SpirvReflectUtil::describeBindings(const QByteArray &spirv)
{
    if (spirv.size() < 20 || (spirv.size() % 4) != 0)
        return QString();

    SpvReflectShaderModule mod{};
    const SpvReflectResult rc = spvReflectCreateShaderModule(spirv.size(), spirv.constData(), &mod);
    if (rc != SPV_REFLECT_RESULT_SUCCESS) {
        spvReflectDestroyShaderModule(&mod);
        return QStringLiteral("(SPIRV-Reflect: could not parse module)\n");
    }

    QStringList lines;
    lines.append(QStringLiteral("SPIRV-Reflect:"));

    uint32_t bindCount = 0;
    spvReflectEnumerateDescriptorBindings(&mod, &bindCount, nullptr);
    QVector<SpvReflectDescriptorBinding *> binds;
    binds.resize(int(bindCount));
    if (bindCount)
        spvReflectEnumerateDescriptorBindings(&mod, &bindCount, binds.data());

    for (uint32_t i = 0; i < bindCount; ++i) {
        const SpvReflectDescriptorBinding *b = binds[int(i)];
        if (!b)
            continue;
        QString type = QStringLiteral("?");
        switch (b->descriptor_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            type = QStringLiteral("UBO");
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            type = QStringLiteral("combined_image_sampler");
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            type = QStringLiteral("SSBO");
            break;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            type = QStringLiteral("storage_image");
            break;
        default:
            break;
        }
        lines.append(QStringLiteral("  set %1 binding %2: %3 (%4)")
                         .arg(b->set)
                         .arg(b->binding)
                         .arg(type)
                         .arg(QString::fromUtf8(b->name ? b->name : "")));
    }

    uint32_t pcCount = 0;
    spvReflectEnumeratePushConstantBlocks(&mod, &pcCount, nullptr);
    if (pcCount > 0)
        lines.append(QStringLiteral("  push constant blocks: %1").arg(pcCount));

    spvReflectDestroyShaderModule(&mod);
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}
