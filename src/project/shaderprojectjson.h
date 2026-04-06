#ifndef SHADERPROJECTJSON_H
#define SHADERPROJECTJSON_H

#include <QString>

class QJsonObject;

namespace ShaderProjectJson {

// Validates `document` against the embedded `schema/shaderproject.schema.json` (subset of JSON Schema).
bool validateAgainstSchema(const QJsonObject &document, QString *errorOut);

} // namespace ShaderProjectJson

#endif
