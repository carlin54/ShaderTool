#ifndef ENUMS_H
#define ENUMS_H

#include <QSet>
#include <QMap>
#include <QString>
#include <vulkan/vulkan.h>


enum class ShaderType {
    Rasterisation,
    RayTracing
};


extern const QMap<VkShaderStageFlagBits, QString> mapShaderStageToGLSLString;
extern const QVector<VkShaderStageFlagBits> shaderStageOrder;
extern const QMap<QString, VkShaderStageFlagBits> mapTabTitleToShaderStage;
extern const QVector<QString> tabOrder;
extern QMap<ShaderType, QVector<VkShaderStageFlagBits>> mapShaderTypes;
extern QMap<ShaderType, QString> mapShaderTypeToString;
extern const QMap<VkShaderStageFlagBits, QString> mapShaderStageToTabTitle;
extern QSet<VkShaderStageFlagBits> MANDATORY_SHADERS;
extern const QString DEFAULT_VERTEX;
extern const QString DEFAULT_GEOMETRY;
extern const QString DEFAULT_FRAGMENT;
extern const QMap<VkShaderStageFlagBits, QString> DEFAULT_RASTERIZER;

#endif // ENUMS_H
