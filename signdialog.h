#ifndef SIGNDIALOG_H
#define SIGNDIALOG_H

#include <QDialog>

namespace Ui {
class SignDialog;
}

class SignDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SignDialog(QWidget *parent = 0);
    ~SignDialog();

    bool init();
private slots:
    void selectFile();
    void sign();
    void cerChanged(int row);
private:
    bool fillSertificateList(const QString &FIO = "0");

    void cleanState();
    void showCertifcateList(bool show = true);

    void signZipFile();
    bool testZipFile();

    Ui::SignDialog *ui;

    QString cspTestFileName;

    QString zipFileName;
    QString unzipFolderName;
    QString engineerFIO;
    QStringList certifcates;
    int     selCertifcate;
};

#endif // SIGNDIALOG_H
