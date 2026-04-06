#ifndef RAYTRACESTAGES_H
#define RAYTRACESTAGES_H

#include <QByteArray>
#include <QString>
#include <QVector>

struct RayTraceStageBinary {
    QString kind; // raygen, miss, closest_hit, any_hit, intersection, callable
    QByteArray spirv;
    QString entry;
};

#endif
