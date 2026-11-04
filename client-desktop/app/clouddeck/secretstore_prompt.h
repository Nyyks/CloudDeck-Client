// Receives org.freedesktop.Secret.Prompt.Completed for SecretStore (Linux only).
#pragma once

#include <QDBusVariant>
#include <QEventLoop>
#include <QObject>

class PromptWatcher : public QObject
{
    Q_OBJECT
public:
    PromptWatcher(QEventLoop* loop, bool* dismissed) : m_Loop(loop), m_Dismissed(dismissed) {}

public slots:
    void completed(bool dismissed, QDBusVariant)
    {
        *m_Dismissed = dismissed;
        m_Loop->quit();
    }

private:
    QEventLoop* m_Loop;
    bool* m_Dismissed;
};
