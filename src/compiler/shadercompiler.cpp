#include "shadercompiler.h"
#include "spirvreflectutil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <vector>

#if __has_include(<dxcapi.h>)
#define SHADERTOOL_HAS_DXCAPI 1
#include <dxcapi.h>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#else
#define SHADERTOOL_HAS_DXCAPI 0
#endif

namespace {

QString defaultDxcompilerLibraryName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("dxcompiler.dll");
#else
    return QStringLiteral("libdxcompiler.so");
#endif
}

QString findDxcompilerLibrary()
{
    const QByteArray env = qgetenv("SHADERTOOL_DXC_LIB");
    if (!env.isEmpty()) {
        const QString p = QString::fromLocal8Bit(env);
        if (QFileInfo::exists(p))
            return p;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString sibling = QDir(appDir).filePath(defaultDxcompilerLibraryName());
    if (QFileInfo::exists(sibling))
        return sibling;

    return QString();
}

#if SHADERTOOL_HAS_DXCAPI
typedef HRESULT(*PFN_DxcCreateInstanceProc)(REFCLSID, REFIID, LPVOID *);

class DxcRuntime
{
public:
    DxcRuntime() = default;
    ~DxcRuntime() { unload(); }

    bool load(QString *err)
    {
        const QString libPath = findDxcompilerLibrary();
        if (libPath.isEmpty()) {
            if (err)
                *err = QStringLiteral("dxcompiler library not found.");
            return false;
        }
#ifdef Q_OS_WIN
        m_h = LoadLibraryW(reinterpret_cast<LPCWSTR>(libPath.utf16()));
        if (!m_h) {
            if (err)
                *err = QStringLiteral("Could not load dxcompiler: %1").arg(libPath);
            return false;
        }
        m_create = reinterpret_cast<PFN_DxcCreateInstanceProc>(GetProcAddress((HMODULE)m_h, "DxcCreateInstance"));
#else
        m_h = dlopen(libPath.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_h) {
            if (err)
                *err = QStringLiteral("Could not load dxcompiler: %1").arg(libPath);
            return false;
        }
        m_create = reinterpret_cast<PFN_DxcCreateInstanceProc>(dlsym(m_h, "DxcCreateInstance"));
#endif
        if (!m_create) {
            if (err)
                *err = QStringLiteral("DxcCreateInstance symbol not found.");
            unload();
            return false;
        }
        return true;
    }

    bool compile(const QString &source, const QString &stageProfile, const QString &entryPoint, ShaderCompileResult *out)
    {
        if (!m_create || !out)
            return false;

        IDxcUtils *utils = nullptr;
        IDxcCompiler3 *compiler = nullptr;
        IDxcResult *result = nullptr;
        IDxcBlobEncoding *srcBlob = nullptr;
        IDxcBlobUtf8 *errBlob = nullptr;
        IDxcBlob *objBlob = nullptr;

        auto cleanup = [&]() {
            if (objBlob)
                objBlob->Release();
            if (errBlob)
                errBlob->Release();
            if (srcBlob)
                srcBlob->Release();
            if (result)
                result->Release();
            if (compiler)
                compiler->Release();
            if (utils)
                utils->Release();
        };

        const HRESULT hUtils = m_create(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        const HRESULT hComp = m_create(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        if (FAILED(hUtils) || FAILED(hComp)) {
            out->stderrText = QStringLiteral("Failed to create DXC interfaces.");
            cleanup();
            return false;
        }

        const QByteArray utf8 = source.toUtf8();
        if (FAILED(utils->CreateBlob(utf8.constData(), UINT32(utf8.size()), CP_UTF8, &srcBlob))) {
            out->stderrText = QStringLiteral("Failed to create DXC source blob.");
            cleanup();
            return false;
        }

        DxcBuffer src{};
        src.Ptr = srcBlob->GetBufferPointer();
        src.Size = srcBlob->GetBufferSize();
        src.Encoding = DXC_CP_UTF8;

        const QString targetEnv = stageProfile == QStringLiteral("lib_6_3")
            ? QStringLiteral("vulkan1.2")
            : QStringLiteral("vulkan1.1");
        const std::wstring wEntry = entryPoint.toStdWString();
        const std::wstring wProfile = stageProfile.toStdWString();
        const std::wstring wTargetEnv = targetEnv.toStdWString();

        std::vector<LPCWSTR> args;
        args.push_back(L"-spirv");
        args.push_back(L"-E");
        args.push_back(wEntry.c_str());
        args.push_back(L"-T");
        args.push_back(wProfile.c_str());
        args.push_back(L"-fspv-target-env");
        args.push_back(wTargetEnv.c_str());

        const HRESULT hCompile = compiler->Compile(&src, args.data(), UINT32(args.size()), nullptr, IID_PPV_ARGS(&result));
        if (FAILED(hCompile) || !result) {
            out->stderrText = QStringLiteral("dxcompiler in-process compile call failed.");
            cleanup();
            return false;
        }

        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errBlob), nullptr);
        if (errBlob && errBlob->GetStringLength() > 0)
            out->stderrText = QString::fromUtf8(errBlob->GetStringPointer());

        HRESULT status = E_FAIL;
        result->GetStatus(&status);
        if (FAILED(status)) {
            if (out->stderrText.trimmed().isEmpty())
                out->stderrText = QStringLiteral("dxcompiler reported failure.");
            cleanup();
            return false;
        }

        if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&objBlob), nullptr)) || !objBlob) {
            out->stderrText = QStringLiteral("dxcompiler produced no SPIR-V object.");
            cleanup();
            return false;
        }

        out->spirv = QByteArray(reinterpret_cast<const char *>(objBlob->GetBufferPointer()),
                                int(objBlob->GetBufferSize()));
        if (out->spirv.size() < 16 || (out->spirv.size() % 4) != 0) {
            out->stderrText = QStringLiteral("Invalid SPIR-V blob size.");
            out->spirv.clear();
            cleanup();
            return false;
        }

        out->ok = true;
        out->stderrText += SpirvReflectUtil::describeBindings(out->spirv);
        cleanup();
        return true;
    }

private:
    void unload()
    {
        if (!m_h)
            return;
#ifdef Q_OS_WIN
        FreeLibrary((HMODULE)m_h);
#else
        dlclose(m_h);
#endif
        m_h = nullptr;
        m_create = nullptr;
    }

    void *m_h = nullptr;
    PFN_DxcCreateInstanceProc m_create = nullptr;
};
#endif // SHADERTOOL_HAS_DXCAPI
} // namespace

QString ShaderCompiler::findDxcExecutable()
{
    const QByteArray env = qgetenv("SHADERTOOL_DXC");
    if (!env.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(env)))
        return QString::fromLocal8Bit(env);

    // Optional hint for bundled layouts: point SHADERTOOL_DXC_LIB at dxcompiler(.dll/.so)
    // and we will try to find a sibling dxc executable in the same directory.
    const QByteArray envLib = qgetenv("SHADERTOOL_DXC_LIB");
    if (!envLib.isEmpty()) {
        const QFileInfo libFi(QString::fromLocal8Bit(envLib));
        const QDir libDir = libFi.isDir() ? QDir(libFi.absoluteFilePath()) : libFi.absoluteDir();
#ifdef Q_OS_WIN
        const QString siblingDxc = libDir.filePath(QStringLiteral("dxc.exe"));
#else
        const QString siblingDxc = libDir.filePath(QStringLiteral("dxc"));
#endif
        if (QFileInfo::exists(siblingDxc))
            return siblingDxc;
    }

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("dxc"));
    if (!fromPath.isEmpty())
        return fromPath;

    static const char *candidates[] = {
        "/usr/bin/dxc",
        "/usr/local/bin/dxc",
        "/opt/vulkan/bin/dxc",
        "/opt/vulkansdk/x86_64/bin/dxc",
    };
    for (const char *c : candidates) {
        if (QFileInfo::exists(QString::fromLatin1(c)))
            return QString::fromLatin1(c);
    }

    const QByteArray vulkanSdk = qgetenv("VULKAN_SDK");
    if (!vulkanSdk.isEmpty()) {
        const QString sdkDxc = QDir(QString::fromLocal8Bit(vulkanSdk)).filePath(QStringLiteral("bin/dxc"));
        if (QFileInfo::exists(sdkDxc))
            return sdkDxc;
    }

    return QString();
}

ShaderCompileResult ShaderCompiler::compileHLSL(const QString &source,
                                                const QString &stageProfile,
                                                const QString &entryPoint)
{
    ShaderCompileResult out;

#if SHADERTOOL_HAS_DXCAPI
    {
        DxcRuntime runtime;
        QString dxErr;
        if (runtime.load(&dxErr)) {
            if (runtime.compile(source, stageProfile, entryPoint, &out))
                return out;
            // in-process path attempted, keep diagnostics and fall back to CLI
            out.stderrText += QStringLiteral("\n[ShaderTool] In-process dxcompiler failed; falling back to dxc CLI.\n");
        }
    }
#endif

    const QString dxc = findDxcExecutable();
    if (dxc.isEmpty()) {
        out.stderrText = QStringLiteral(
            "Could not find `dxc` (DirectX Shader Compiler).\n\n"
            "To fix this, do ONE of the following:\n"
            "  1. Install the Vulkan SDK: https://vulkan.lunarg.com/sdk/home\n"
            "     (Ubuntu: sudo apt install vulkan-sdk)\n"
            "  2. Install dxc standalone: sudo apt install dxc\n"
            "  3. Set the environment variable SHADERTOOL_DXC=/path/to/dxc\n"
            "  4. Place the dxc binary on your PATH");
        return out;
    }

    QTemporaryFile hlslFile(QStringLiteral("%1/ShaderTool_XXXXXX.hlsl").arg(QDir::tempPath()));
    hlslFile.setAutoRemove(true);
    if (!hlslFile.open()) {
        out.stderrText = QStringLiteral("Failed to create temp HLSL file.");
        return out;
    }
    hlslFile.write(source.toUtf8());
    hlslFile.flush();
    const QString hlslPath = hlslFile.fileName();

    QTemporaryFile spvFile(QStringLiteral("%1/ShaderTool_XXXXXX.spv").arg(QDir::tempPath()));
    spvFile.setAutoRemove(true);
    if (!spvFile.open()) {
        out.stderrText = QStringLiteral("Failed to create temp SPIR-V file.");
        return out;
    }
    const QString spvPath = spvFile.fileName();
    spvFile.close();

    QProcess proc;
    proc.setProgram(dxc);
    QStringList args;
    args << QStringLiteral("-spirv")
         << QStringLiteral("-T") << stageProfile
         << QStringLiteral("-E") << entryPoint;
    if (stageProfile == QStringLiteral("lib_6_3"))
        args << QStringLiteral("-fspv-target-env=vulkan1.2");
    else
        args << QStringLiteral("-fspv-target-env=vulkan1.1");
    args << QStringLiteral("-Fo") << spvPath << hlslPath;
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        out.stderrText = QStringLiteral("dxc timed out.");
        return out;
    }

    out.stderrText = QString::fromUtf8(proc.readAllStandardError());
    if (proc.exitCode() != 0) {
        if (out.stderrText.trimmed().isEmpty())
            out.stderrText = QStringLiteral("dxc failed with exit code %1.").arg(proc.exitCode());
        return out;
    }

    QFile readSpv(spvPath);
    if (!readSpv.open(QIODevice::ReadOnly)) {
        out.stderrText = QStringLiteral("Failed to read generated SPIR-V.");
        return out;
    }
    out.spirv = readSpv.readAll();
    readSpv.close();
    if (out.spirv.size() < 16 || (out.spirv.size() % 4) != 0) {
        out.stderrText = QStringLiteral("Invalid SPIR-V blob size.");
        out.spirv.clear();
        return out;
    }

    out.ok = true;
    out.stderrText += SpirvReflectUtil::describeBindings(out.spirv);
    return out;
}
