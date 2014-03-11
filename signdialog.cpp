#include "signdialog.h"
#include "ui_signdialog.h"

#include <QSettings>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
#include <QMessageBox>
#include <QUuid>
#include <QDir>
#include <QFileDialog>
#include <QDebug>
#include <QDomDocument>

int runCmd(const QString &cmd)
{
    QProcess p;
    p.start(cmd);
    p.waitForFinished();
    return p.exitCode();
}

QString findFIOInXML(const QDomNode root)
{
    QString result = "";
    QDomNodeList childNodes = root.childNodes();
    for(int i = 0; i < childNodes.count(); ++i)
    {
        QDomNode n = childNodes.at(i);
        if(n.nodeName() == "Cadastral_Engineer" || n.nodeName() == "Contractor") {
            QDomNode fio = n.firstChildElement("FIO");
            if(!fio.isNull()) {
                QDomElement f = fio.firstChildElement("Surname");
                if(!f.isNull()) {
                    result += f.text();
                }
                QDomElement i = fio.firstChildElement("First");
                if(!i.isNull()) {
                    result += " " + i.text();
                }
                QDomElement o = fio.firstChildElement("Patronymic");
                if(!i.isNull()) {
                    result += " " + o.text();
                }
            }
        }
        else if(n.nodeName() == "Sender") {
            QDomNode a = n.attributes().namedItem("Name");
            if(!a.isNull()) {
                result += a.nodeValue();
            }
        }
        else {
            result = findFIOInXML(n);
        }
        if("" != result)
            break;
    }
    return result;
}

bool extractResFile(const QString &resName, const QString &outName)
{
    bool result = true;
    if(!QFile::exists(outName)) {
        QFile res(resName);
        result = res.copy(outName);
    }
    return result;
}

bool SignDialog::init()
{
    if(!extractResFile(":/res/7z.dll", QDir::tempPath() + "/" + "7z.dll") ||
       !extractResFile(":/res/7z.exe", QDir::tempPath() + "/" + "7z.exe"))
    {
        ui->log->appendPlainText("Не найден 7z.");
        return false;
    }

    cspTestFileName = "";

    QString cspAppPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Crypto Pro\\Cryptography\\CurrentVersion\\AppPath";
    QSettings cspSettings(cspAppPath, QSettings::NativeFormat);

    if(cspSettings.childGroups().size() > 0) {
        QSettings anySettings(cspAppPath + "\\" + cspSettings.childGroups().at(0), QSettings::NativeFormat);
        QString csptest = QFileInfo(anySettings.value("Default", "").toString()).canonicalPath() + "/csptest.exe";

        if(QFile::exists(csptest))
            cspTestFileName = csptest;
    }

    if(cspTestFileName == "") {
        ui->log->appendPlainText("Не найден Crypto Pro.");
        return false;
    }
    return true;
}

SignDialog::SignDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignDialog)
{
    ui->setupUi(this);
    setLayout(ui->verticalLayout);

    setWindowIcon(QIcon("://res/app.png"));

    connect(ui->selZip, SIGNAL(clicked()), this, SLOT(selectFile()) );
    connect(ui->signBtn, SIGNAL(clicked()), this, SLOT(sign()) );
    connect(ui->sertList,  SIGNAL(currentRowChanged(int)), this, SLOT(cerChanged(int)));

    cleanState();

    if(!init())
        ui->selZip->setDisabled(true);
}

SignDialog::~SignDialog()
{
    cleanState();
    delete ui;
}

void SignDialog::selectFile()
{
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    cleanState();

    QString selfilter = "Файл (*.zip)";
    zipFileName = QFileDialog::getOpenFileName(qApp->activeWindow(), "Aрхив", QString(), "Файл (*.zip)", &selfilter);
    ui->log->appendPlainText("Файл: " + zipFileName);
    if(testZipFile()) {

        ui->log->appendPlainText("Инжeнер:" + engineerFIO);

        if(selCertifcate >= 0) {
            signZipFile();
        }
        else {
            showCertifcateList(true);
        }
    }
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif
}

void SignDialog::sign()
{
#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
    signZipFile();
#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif
}

bool SignDialog::testZipFile()
{
    if(zipFileName == "") {
        ui->log->appendPlainText("Не выбран zip файл.");
        return false;
    }
    if(!QFile::exists(zipFileName)) {
        ui->log->appendPlainText("Не найден zip файл.");
        return false;
    }
    zipFileName = " \"" + zipFileName + "\" ";

    QString curId = QUuid::createUuid().toString();
    QDir::temp().mkdir(curId);
    unzipFolderName = QDir::tempPath() + "\\" + curId;
    QDir unzipFolder(unzipFolderName);

    QString exe7zName = "\"" + QDir::tempPath() + "\\7z.exe\" ";

    if(runCmd(exe7zName + " e -i!*.sig -o" + unzipFolderName + zipFileName) != 0) {
        ui->log->appendPlainText("Не удалось проверить архив.");
        return false;
    }

    if(unzipFolder.entryList(QDir::nameFiltersFromString("*.sig")).size() > 0) {
        ui->log->appendPlainText("Файл уже содержит подписи.");
        return false;
    }

    if(runCmd(exe7zName + " e -o" + unzipFolderName + zipFileName) != 0) {
        ui->log->appendPlainText("Не удалось распаковать файлы.");
        return false;
    }

    QStringList xmls = unzipFolder.entryList(QDir::nameFiltersFromString("*.xml"));
    if(xmls.size() < 1) {
        ui->log->appendPlainText("Не найден xml файл.");
        return false;
    }

    QFile xmlFile(unzipFolder.absolutePath() + "\\" + xmls.first());
    if (!xmlFile.open(QIODevice::ReadOnly))
    {
        ui->log->appendPlainText("Ошибка при открытии xml файла.");
        return false;
    }

    QDomDocument xml;
    if (!xml.setContent(&xmlFile))
    {
        ui->log->appendPlainText("Неправильный xml файл.");
        return false;
    }
    xmlFile.close();

    engineerFIO = findFIOInXML(xml.firstChildElement());
    if(engineerFIO == "") {
        ui->log->appendPlainText("Не найдены ФИО кадастрового инжинера.");
        return false;
    }

    engineerFIO = "\"" + engineerFIO + "\"";

    if(!fillSertificateList(engineerFIO)) {
        ui->log->appendPlainText("Не найдены сертификаты.");
        return false;
    }

    if(certifcates.size() == 1) {
        selCertifcate = 0;
        showCertifcateList(false);
    }
    else {
        selCertifcate = -1;
        showCertifcateList(true);
    }

    return true;
}

void SignDialog::cleanState()
{
    zipFileName = "";

    if(unzipFolderName != "" && QFileInfo(unzipFolderName).exists())
    {
        QDir uz(unzipFolderName);
        foreach (QString name, uz.entryList()) {
            QFile f(unzipFolderName + "\\" + name);
            f.remove();
        }
        uz.rmdir(unzipFolderName);
    }

    unzipFolderName = "";

    engineerFIO = "";
    selCertifcate = -1;

    showCertifcateList(false);
    ui->log->clear();
}

void SignDialog::showCertifcateList(bool show)
{
    ui->signBtn->setEnabled(false);

    ui->signBtn->setVisible(show);
    ui->labelSert->setVisible(show);
    ui->sertList->setVisible(show);
    if(show) {
        ui->sertList->clear();
        foreach(QString c, certifcates) {
            ui->sertList->addItem(c);
        }
    }
}

void SignDialog::cerChanged(int row)
{
    selCertifcate = row;
    if(row >=0 )
        ui->signBtn->setEnabled(true);
}

void SignDialog::signZipFile()
{
    QString tmpFileName = QDir::tempPath() + "/" + QUuid::createUuid().toString();
    QString batFileName = tmpFileName + ".bat";
    QString inFileName = tmpFileName + ".in";

    QFile   inFile(inFileName);
    inFile.open(QFile::WriteOnly);
    QTextStream inStream(&inFile);
    inStream << selCertifcate;
    inFile.close();

    QFile   batFile(batFileName);
    batFile.open(QFile::WriteOnly);
    QTextStream batStream(&batFile);
    batStream << "chcp 1251\n";
    batStream << "FOR /R " + unzipFolderName + " %%i IN (*.*) DO (\"" + cspTestFileName + "\"" + " -sfsign -sign -in \"%%~fi\" -out \"%%~fi.sig\" -my " + engineerFIO + " -addsigtime -add -detached < \"" + inFileName + "\" )\n";
    batStream.flush();
    batFile.close();

    QProcess p;
    p.start(batFileName);
    p.waitForFinished();

    batFile.remove();

    if(p.exitCode() != 0) {
        ui->log->appendPlainText("Во время подписи возникли ошибки, файлы не изменены.");
    }
    else {
        batFile.open(QFile::WriteOnly);
        batStream.setDevice(&batFile);
        batStream << "chcp 1251\n";
        batStream << "\"" + QDir::tempPath() + "\\7z.exe\"" + " a -tzip " + zipFileName + " \"" + unzipFolderName + "\\*.sig\"";
        batStream.flush();
        batFile.close();

        p.start(batFileName);
        p.waitForFinished();
        if(p.exitCode() != 0) {
            ui->log->appendPlainText("Во время подписи возникли ошибки, проверте zip файл.");
        }
        else {
            ui->log->appendPlainText("Все операции успешно завершены, в zip файл добавлены подписи.");
        }
    }

    batFile.remove();
    inFile.remove();

    showCertifcateList(false);
}

bool SignDialog::fillSertificateList(const QString &FIO)
{
    selCertifcate = -1;
    certifcates.clear();

    QString tmpFileName = QDir::tempPath() + "/" + QUuid::createUuid().toString();
    QString batFileName = tmpFileName + ".bat";
    QString inFileName = tmpFileName + ".in";
    QString signFileName = tmpFileName + ".sig";
    QString infFileName = tmpFileName + ".info";

    QFile   inFile(inFileName);
    inFile.open(QFile::WriteOnly);
    QTextStream inStream(&inFile);
    inStream << "0";
    inFile.close();

    QFile   batFile(batFileName);
    batFile.open(QFile::WriteOnly);
    QTextStream batStream(&batFile);
    batStream << "chcp 1251\n";
    batStream << "\"" + cspTestFileName + "\"" + " -sfsign -sign -in " + batFileName + " -out " + signFileName + " -my " + FIO + " -addsigtime -add -detached < \"" + inFileName + "\" > \"" + infFileName + "\"";
    batFile.close();

    QProcess p;
    p.start(batFileName);
    p.waitForFinished();

    inFile.remove();
    batFile.remove();
    QFile signFile(signFileName);
    signFile.remove();

    QFile infFile(infFileName);
    if(!infFile.open(QFile::ReadOnly)) {
        return false;
    }

    QTextStream stream(&infFile);

    bool result = false;
    QString line;
    QString cert = "";
    do {
        line = stream.readLine();
        QRegExp sub("Subject.*CN=([^,]*).*");
        QRegExp valid("Valid[^-]*-\\s*([\\d\\.]*).*");

        int pos = sub.indexIn(line);
        if (pos > -1)
        {
            if(cert != "") {
                 certifcates.append(cert);
                 cert = "";
                 result = true;
            }
            cert = sub.cap(1);
            continue;
        }

        pos = valid.indexIn(line);
        if (pos > -1)
        {
            cert += " (действителен до " + valid.cap(1) + ")";
        }
    } while (!line.isNull());

    if(cert != "") {
         certifcates.append(cert);
         result = true;
    }

    infFile.close();
    infFile.remove();
    return result;
}



