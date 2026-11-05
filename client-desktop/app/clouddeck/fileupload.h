// CloudDeck-Client: uploads files to the CloudDeck's Downloads/CloudDeck folder
// (Phase 5, docs/adr/0011-file-upload.md). Files are dropped onto the stream window
// or picked from the in-stream menu. One worker thread works through a queue, one
// agent tunnel per file, and reports progress with signals.
#pragma once

#include <QMutex>
#include <QSslCertificate>
#include <QStringList>
#include <QThread>

class FileUploader : public QThread
{
    Q_OBJECT
public:
    FileUploader(QString host, quint16 httpsPort, QSslCertificate serverCert);
    ~FileUploader() override; // aborts a running upload (the VM keeps nothing of it)

    // Any thread. Starts the worker if it isn't running.
    void enqueue(const QStringList& localPaths);

signals:
    void progress(QString text);                          // e.g. "Uploading a.pdf (2 of 3): 45%"
    void batchFinished(QString summary, bool allSucceeded);

protected:
    void run() override;

private:
    // Returns the saved path on the VM, or sets *error.
    QString uploadOne(const QString& path, int index, int total, QString* error);

    const QString m_Host;
    const quint16 m_Port;
    const QSslCertificate m_ServerCert;

    QMutex m_Lock;
    QStringList m_Queue;
    int m_BatchTotal = 0;
    bool m_Active = false; // a run is (about to be) working through the queue
};
