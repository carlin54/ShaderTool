#include "exportdialog.h"
#include "previewvulkanwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QVBoxLayout>

ExportDialog::ExportDialog(PreviewVulkanWindow *preview, QWidget *parent)
    : QDialog(parent)
    , m_preview(preview)
{
    setWindowTitle(QStringLiteral("Export animation"));
    setMinimumWidth(420);

    m_durationSec = new QSpinBox;
    m_durationSec->setRange(1, 600);
    m_durationSec->setValue(5);
    m_fps = new QSpinBox;
    m_fps->setRange(1, 120);
    m_fps->setValue(30);
    m_format = new QComboBox;
    m_format->addItem(QStringLiteral("WebP (animated)"), QStringLiteral("webp"));
    m_format->addItem(QStringLiteral("GIF"), QStringLiteral("gif"));
    m_outputPath = new QLineEdit;
    m_outputPath->setPlaceholderText(QStringLiteral("Output file path…"));

    auto *browse = new QPushButton(QStringLiteral("Browse…"));
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            QStringLiteral("WebP (*.webp);;GIF (*.gif);;All (*)"));
        if (!path.isEmpty())
            m_outputPath->setText(path);
    });

    auto *pathLay = new QHBoxLayout;
    pathLay->addWidget(m_outputPath, 1);
    pathLay->addWidget(browse);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Duration (seconds):"), m_durationSec);
    form->addRow(QStringLiteral("FPS:"), m_fps);
    form->addRow(QStringLiteral("Format:"), m_format);
    form->addRow(QStringLiteral("Output:"), pathLay);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &ExportDialog::runExport);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(new QLabel(
        QStringLiteral("Records the Vulkan preview using QVulkanWindow::grab() with deterministic "
                       "simulation time. Requires ffmpeg on PATH for encoding.")));
    lay->addLayout(form);
    lay->addWidget(buttons);
}

void ExportDialog::runExport()
{
    if (!m_preview) {
        reject();
        return;
    }
    if (!m_preview->supportsGrab()) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("This platform/GPU does not support grab() for the preview."));
        return;
    }

    QString outPath = m_outputPath->text().trimmed();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Export"), QStringLiteral("Choose an output file."));
        return;
    }

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("Export"),
            QStringLiteral("Could not find `ffmpeg` on PATH. Install ffmpeg to encode GIF/WebP."));
        return;
    }

    const int durationSec = m_durationSec->value();
    const int fps = m_fps->value();
    const int totalFrames = durationSec * fps;
    const QString fmt = m_format->currentData().toString();

    if (fmt == QStringLiteral("gif") && !outPath.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".gif");
    if (fmt == QStringLiteral("webp") && !outPath.endsWith(QStringLiteral(".webp"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".webp");

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Export"), QStringLiteral("Could not create temp directory."));
        return;
    }

    QProgressDialog progress(QStringLiteral("Capturing frames…"), QStringLiteral("Cancel"), 0, totalFrames,
                             this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    const float prevOverride = m_preview->simulationTimeOverrideMs();
    for (int i = 0; i < totalFrames; ++i) {
        progress.setValue(i);
        if (progress.wasCanceled()) {
            m_preview->setSimulationTimeOverrideMs(prevOverride);
            return;
        }

        const float tMs = float(i) * (1000.f / float(fps));
        m_preview->setSimulationTimeOverrideMs(tMs);
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        m_preview->requestUpdate();
        QApplication::processEvents(QEventLoop::AllEvents, 50);

        QImage img = m_preview->grab();
        if (img.isNull()) {
            m_preview->setSimulationTimeOverrideMs(prevOverride);
            QMessageBox::warning(this, QStringLiteral("Export"), QStringLiteral("grab() returned an empty image."));
            return;
        }

        const QString framePath = tempDir.path() + QStringLiteral("/frame_%1.png").arg(i, 5, 10, QChar(QLatin1Char('0')));
        if (!img.save(framePath, "PNG")) {
            m_preview->setSimulationTimeOverrideMs(prevOverride);
            QMessageBox::warning(this, QStringLiteral("Export"),
                                 QStringLiteral("Failed to save frame %1.").arg(i));
            return;
        }
    }
    progress.setValue(totalFrames);
    m_preview->setSimulationTimeOverrideMs(prevOverride);

    QStringList ffArgs;
    ffArgs << QStringLiteral("-y")
           << QStringLiteral("-framerate") << QString::number(fps) << QStringLiteral("-i")
           << tempDir.path() + QStringLiteral("/frame_%05d.png");

    if (fmt == QStringLiteral("webp")) {
        ffArgs << QStringLiteral("-c:v") << QStringLiteral("libwebp_anim") << QStringLiteral("-qscale:v")
               << QStringLiteral("75") << outPath;
    } else {
        ffArgs << QStringLiteral("-vf")
               << QStringLiteral("fps=%1,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse").arg(fps)
               << outPath;
    }

    QProcess proc;
    proc.start(ffmpeg, ffArgs);
    if (!proc.waitForFinished(120000)) {
        proc.kill();
        QMessageBox::warning(this, QStringLiteral("Export"), QStringLiteral("ffmpeg timed out."));
        return;
    }
    if (proc.exitCode() != 0) {
        QMessageBox::warning(
            this, QStringLiteral("Export"),
            QStringLiteral("ffmpeg failed:\n%1").arg(QString::fromUtf8(proc.readAllStandardError())));
        return;
    }

    QMessageBox::information(this, QStringLiteral("Export"),
                             QStringLiteral("Wrote:\n%1").arg(outPath));
    accept();
}
