#pragma once

#include <QObject>
#include <QPointer>

class RapooAiKeyAdapter final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit RapooAiKeyAdapter(QObject *parent = nullptr);

    bool isAvailable() const { return m_available; }
    QString status() const { return m_status; }

    // Bridges the recovered Rapoo signal when a compatible device object is supplied.
    // Without that object the adapter deliberately remains an unavailable, non-fatal stub.
    bool attachDeviceSource(QObject *source);
    void detachDeviceSource();

public slots:
    bool start();
    void stop();

signals:
    void activated();
    void availabilityChanged(bool available);
    void statusChanged(const QString &status);
    void unavailable(const QString &reason);

private:
    void setAvailability(bool available, const QString &status);

    QPointer<QObject> m_source;
    QMetaObject::Connection m_deviceConnection;
    QMetaObject::Connection m_destroyedConnection;
    bool m_available = false;
    QString m_status;
};
