#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>

class PreviewVulkanWindow;
class QSpinBox;
class QComboBox;
class QLineEdit;

class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(PreviewVulkanWindow *preview, QWidget *parent = nullptr);

private:
    void runExport();

    PreviewVulkanWindow *m_preview = nullptr;
    QSpinBox *m_durationSec = nullptr;
    QSpinBox *m_fps = nullptr;
    QComboBox *m_format = nullptr;
    QLineEdit *m_outputPath = nullptr;
};

#endif
