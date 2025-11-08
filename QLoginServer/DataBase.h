#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>

class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    bool initialize();

    // User management
    bool createUser(const QString &username, const QString &passwordHash);
    bool userExists(const QString &username);
    QString getUserPasswordHash(const QString &username);
    QString getUserId(const QString &username);

    // User data management
    bool saveUserData(const QString &userId, const QString &dataKey, const QJsonObject &data);
    QJsonObject getUserData(const QString &userId, const QString &dataKey);
    QJsonArray getAllUserData(const QString &userId);
    bool deleteUserData(const QString &userId, const QString &dataKey);

private:
    QSqlDatabase m_db;
    QMutex m_mutex;  // Thread safety for concurrent access

    bool createTables();
};

#endif // DATABASE_H
