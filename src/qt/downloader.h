#ifndef DOWNLOADER_H
#define DOWNLOADER_H

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

class Downloader : public QObject {
    Q_OBJECT
public:
    //    Downloader(QObject *parent = 0);
    virtual ~Downloader();
    void SetProgress(QString title, QString description);
    void doDownload(QString url, QString dir = QString(), QString filename = QString(), bool savefile = false);
    void executeFile();
    bool TryUpdate(int clientversion_);
    QString GetInstallerExt();


Q_SIGNALS:
    void downloaded(int code);

    public
Q_SLOTS:
    //  void replyFinished();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void CheckedUpdates(QNetworkReply * reply_);
    void DownloadedUpdates(QNetworkReply * reply_);

private:
    QNetworkAccessManager *manager;
    QNetworkRequest req;
    QUrl url;
    QString dir;
    QString filename;
    QString urlstr;
    QByteArray data;
    QProgressDialog *progress = 0;
    QByteArray GetData();
    QString GetFullpath();
    QNetworkReply *reply;
    QString fullpath;
    qint64 DataSize = 0;
    bool savefile;
    bool abort = false;
    bool useprogress = false;
    int clientversion;

};

#endif // DOWNLOADER_H