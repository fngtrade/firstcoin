#include "downloader.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QString>
#include <QByteArray>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QDebug>
#include <QApplication>
#include <QPushButton>
#include <QList>

//Downloader::Downloader(QObject *parent) :
//QObject(parent) {
//}

Downloader::~Downloader() {
}

QByteArray Downloader::GetData() {
    return this->data;
}

QString Downloader::GetFullpath() {
    return this->fullpath;
}

QString Downloader::GetInstallerExt() {
#if defined(Q_OS_MAC)
    return QString(".dmg");
#elif defined(Q_OS_WIN64)
    return QString("64.exe");
#elif defined(Q_OS_WIN32)
    return QString("32.exe");
#elif defined(Q_OS_LINUX)
    return QString(".rpm");
#endif
    return QString("");
}

void Downloader::SetProgress(QString title, QString description) {
    this->useprogress = true;
    this->progress = new QProgressDialog();
    this->progress->setMinimumDuration(0);
    this->progress->setWindowTitle(title);
    this->progress->setLabelText(description);
    this->progress->setRange(0, 1);
    QList<QPushButton *> L = this->progress->findChildren<QPushButton *>();
    L.at(0)->hide();

}

void Downloader::doDownload(QString url, QString dir, QString filename, bool savefile) {
    if (this->abort)
        return;
    manager = new QNetworkAccessManager(this);
    if (!manager) {
        return;
    }
    this->urlstr = url;
    this->url = QUrl(this->urlstr);
    this->dir = dir == QString("") ? QDir::tempPath() : dir;
    this->req = QNetworkRequest(this->url);
    this->reply = manager->get(this->req);
    if (!this->reply) {
        return;
    }

    //QDir::toNativeSeparators(this->dir);
    if (this->dir[this->dir.length() - 1] != QDir::separator()) {
        this->dir += QDir::separator();
    }

    this->filename = filename;

    this->fullpath = this->dir + this->filename;
    //connect(this->reply, SIGNAL(finished()), this, SLOT(replyFinished()));
    if (!savefile) {
        connect(this->manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(CheckedUpdates(QNetworkReply *)));
    } else {
        connect(this->manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(DownloadedUpdates(QNetworkReply *)));
    }
    connect(this->reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));

    //manager->get(QNetworkRequest(this->url));
}

void Downloader::executeFile() {
    if (this->abort)
        return;
    //  QString t = QDir::toNativeSeparators("file:///" + this->GetFullpath());
    //  qDebug() << "Downloader::executeFile()" << t;
    QDesktopServices::openUrl(QUrl::fromLocalFile("file:///" + this->GetFullpath()));
    QApplication::quit();
}

//void Downloader::replyFinished() {
//    if (reply->error()) {
//        qDebug() << "Updates download error:" << reply->errorString();
//    } else {
//        qDebug() << this->reply->header(QNetworkRequest::ContentTypeHeader).toString();
//        qDebug() << this->reply->header(QNetworkRequest::ContentLengthHeader).toULongLong();
//        qDebug() << this->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
//        if (this->savefile) {
//            QFile *file = new QFile(this->fullpath);
//            if (file->open(QFile::WriteOnly)) {
//                file->write(reply->readAll());
//                file->flush();
//                file->close();
//            }
//            delete file;
//        } else {
//            this->data = this->reply->readAll();
//        }
//        emit downloaded(this->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
//    }
//
//    reply->deleteLater();
//}

bool Downloader::TryUpdate(int clientversion_) { //true=OK, run app; false=run installer and exit
    this->clientversion = clientversion_;
    QString CHECKURL = QString("https://firstcoin.world/updates/lastversion.txt");
    // connect(this->downloader, SIGNAL(downloaded(int)), this, SLOT(CheckedUpdates(int)));
    doDownload(CHECKURL, QString(""), QString(""), false);
}

void Downloader::CheckedUpdates(QNetworkReply * reply_) {
    const QString DOWNLOADURL = QString("https://firstcoin.world/updates/firstcoin-setup-latest") + GetInstallerExt();
    const QString SAVEFILENAME = QString("firstcoin-setup-latest");
    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!statusCode.isValid()) {
        this->abort = true;
    }
    int status = statusCode.toInt();

    if (status != 200) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        qDebug() << reason;
        this->abort = true;
    }
    if (this->abort)
        return;
    int latest = reply->readAll().toInt();
    if (latest > this->clientversion) {
        SetProgress("Обновление", "Загрузка обновлений...");
        disconnect(this->manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(CheckedUpdates(QNetworkReply *)));
        doDownload(DOWNLOADURL, QString(), QString(SAVEFILENAME + GetInstallerExt()), true);
    }
}

void Downloader::DownloadedUpdates(QNetworkReply * reply_) {

    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!statusCode.isValid()) {
        this->abort = true;
    }
    int status = statusCode.toInt();

    if (status != 200) {
        QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        qDebug() << reason;
        this->abort = true;
    }
    if (this->abort)
        return;

    QFile::remove(fullpath);
    QFile *file = new QFile(fullpath);
    if (file->open(QFile::WriteOnly)) {
        file->write(reply_->readAll());
        file->flush();
        file->close();
    }
    // delete file;
    executeFile();
}

void Downloader::downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (this->progress) {
        if (this->useprogress) {
            if (this->progress->maximum() == 1) {
                this->progress->setRange(0, bytesTotal);
            }
            this->progress->setValue(bytesReceived);
            // this->progress->processEvents();
            if (this->progress->wasCanceled()) {

            }
        }
    }
}

