#include "shaderprojectbundle.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QDataStream>
#include <QIODevice>


namespace {

quint32 crc32Table[256];
bool crc32TableInit = false;

void initCrc32Table()
{
    if (crc32TableInit) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        crc32Table[i] = crc;
    }
    crc32TableInit = true;
}

quint32 computeCrc32(const QByteArray &data)
{
    initCrc32Table();
    quint32 crc = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); i++)
        crc = crc32Table[(crc ^ quint8(data[i])) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

bool writeZip(const QString &zipPath, const QVector<QPair<QString, QByteArray>> &entries, QString *errorOut)
{
    QFile f(zipPath);
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Cannot open output file: ") + zipPath;
        return false;
    }

    struct CentralEntry {
        quint32 localOffset;
        QByteArray nameUtf8;
        quint32 crc;
        quint32 size;
    };
    QVector<CentralEntry> central;

    QDataStream s(&f);
    s.setByteOrder(QDataStream::LittleEndian);

    for (const auto &entry : entries) {
        CentralEntry ce;
        ce.localOffset = quint32(f.pos());
        ce.nameUtf8 = entry.first.toUtf8();
        ce.crc = computeCrc32(entry.second);
        ce.size = quint32(entry.second.size());

        // Local file header
        s << quint32(0x04034b50); // signature
        s << quint16(20);         // version needed
        s << quint16(0);          // flags
        s << quint16(0);          // compression (stored)
        s << quint16(0);          // mod time
        s << quint16(0);          // mod date
        s << ce.crc;
        s << ce.size;             // compressed size
        s << ce.size;             // uncompressed size
        s << quint16(ce.nameUtf8.size());
        s << quint16(0);          // extra field length
        f.write(ce.nameUtf8);
        f.write(entry.second);

        central.append(ce);
    }

    quint32 centralStart = quint32(f.pos());
    for (const CentralEntry &ce : central) {
        s << quint32(0x02014b50); // central directory signature
        s << quint16(20);         // version made by
        s << quint16(20);         // version needed
        s << quint16(0);          // flags
        s << quint16(0);          // compression
        s << quint16(0);          // mod time
        s << quint16(0);          // mod date
        s << ce.crc;
        s << ce.size;             // compressed size
        s << ce.size;             // uncompressed size
        s << quint16(ce.nameUtf8.size());
        s << quint16(0);          // extra field length
        s << quint16(0);          // file comment length
        s << quint16(0);          // disk number start
        s << quint16(0);          // internal file attributes
        s << quint32(0);          // external file attributes
        s << ce.localOffset;
        f.write(ce.nameUtf8);
    }

    quint32 centralEnd = quint32(f.pos());
    // End of central directory
    s << quint32(0x06054b50);
    s << quint16(0); // disk number
    s << quint16(0); // disk with central dir
    s << quint16(central.size());
    s << quint16(central.size());
    s << quint32(centralEnd - centralStart);
    s << centralStart;
    s << quint16(0); // comment length

    f.close();
    return true;
}

bool readZip(const QString &zipPath, QVector<QPair<QString, QByteArray>> *entries, QString *errorOut)
{
    QFile f(zipPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Cannot open bundle: ") + zipPath;
        return false;
    }

    QByteArray allData = f.readAll();
    f.close();

    const char *d = allData.constData();
    qint64 size = allData.size();
    qint64 pos = 0;

    while (pos + 30 <= size) {
        quint32 sig = *reinterpret_cast<const quint32 *>(d + pos);
        if (sig != 0x04034b50)
            break;

        quint16 compression = *reinterpret_cast<const quint16 *>(d + pos + 8);
        quint32 compSize = *reinterpret_cast<const quint32 *>(d + pos + 18);
        quint32 uncompSize = *reinterpret_cast<const quint32 *>(d + pos + 22);
        quint16 nameLen = *reinterpret_cast<const quint16 *>(d + pos + 26);
        quint16 extraLen = *reinterpret_cast<const quint16 *>(d + pos + 28);

        pos += 30;
        if (pos + nameLen > size) break;
        QString name = QString::fromUtf8(d + pos, nameLen);
        pos += nameLen + extraLen;

        if (compression != 0) {
            if (errorOut) *errorOut = QStringLiteral("Compressed entries not supported in this reader.");
            return false;
        }

        if (pos + compSize > size) break;
        QByteArray data(d + pos, int(compSize));
        pos += compSize;

        Q_UNUSED(uncompSize);
        entries->append({name, data});
    }

    return true;
}

} // anonymous namespace

bool ShaderProjectBundle::isBundle(const QString &path)
{
    return path.endsWith(QStringLiteral(".stproj"), Qt::CaseInsensitive);
}

ShaderProjectBundle::ExtractResult ShaderProjectBundle::extractToTemp(const QString &stprojPath)
{
    ExtractResult result;

    QVector<QPair<QString, QByteArray>> entries;
    if (!readZip(stprojPath, &entries, &result.errorMessage))
        return result;

    QTemporaryDir tmpDir;
    tmpDir.setAutoRemove(false);
    if (!tmpDir.isValid()) {
        result.errorMessage = QStringLiteral("Failed to create temporary directory.");
        return result;
    }
    result.tempDir = tmpDir.path();

    bool foundProject = false;
    for (const auto &entry : entries) {
        const QString &name = entry.first;
        const QByteArray &data = entry.second;

        if (name.endsWith(QLatin1Char('/')))
            continue;

        QString filePath = result.tempDir + QLatin1Char('/') + name;
        QDir().mkpath(QFileInfo(filePath).absolutePath());
        QFile out(filePath);
        if (!out.open(QIODevice::WriteOnly)) {
            result.errorMessage = QStringLiteral("Failed to extract: ") + name;
            return result;
        }
        out.write(data);
        out.close();

        if (name == QStringLiteral("project.json")) {
            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
            if (doc.isObject()) {
                result.projectJson = doc.object();
                foundProject = true;
            }
        }
    }

    if (!foundProject) {
        result.errorMessage = QStringLiteral("Bundle does not contain project.json.");
        return result;
    }

    result.ok = true;
    return result;
}

bool ShaderProjectBundle::createBundle(const QString &outputPath, const QString &projectJsonPath,
                                       const QStringList &assetPaths, QString *errorOut)
{
    QVector<QPair<QString, QByteArray>> entries;

    QFile projFile(projectJsonPath);
    if (!projFile.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("Cannot read project file.");
        return false;
    }
    entries.append({QStringLiteral("project.json"), projFile.readAll()});
    projFile.close();

    for (const QString &assetPath : assetPaths) {
        QFile assetFile(assetPath);
        if (!assetFile.open(QIODevice::ReadOnly))
            continue;
        QString relativeName = QStringLiteral("assets/") + QFileInfo(assetPath).fileName();
        entries.append({relativeName, assetFile.readAll()});
    }

    return writeZip(outputPath, entries, errorOut);
}

void ShaderProjectBundle::cleanupTempDir(const QString &tempDir)
{
    if (tempDir.isEmpty())
        return;
    QDir dir(tempDir);
    if (dir.exists())
        dir.removeRecursively();
}
