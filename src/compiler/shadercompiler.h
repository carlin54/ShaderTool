#ifndef SHADERCOMPILER_H
#define SHADERCOMPILER_H

#include <QString>
#include <QByteArray>

struct ShaderCompileResult {
    bool ok = false;
    QByteArray spirv;
    QString stderrText;
};

namespace ShaderCompiler {

// HLSL → SPIR-V. Tries in-process dxcompiler when available, otherwise falls back to `dxc` CLI.
ShaderCompileResult compileHLSL(const QString &source,
                                const QString &stageProfile,
                                const QString &entryPoint);

QString findDxcExecutable();

} // namespace ShaderCompiler

#endif
