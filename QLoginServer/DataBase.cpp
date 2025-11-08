#include "DataBase.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QUuid>

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool Database::initialize()
{
    QMutexLocker locker(&m_mutex);

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("userdata.db");

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    qInfo() << "Database opened successfully";
    return createTables();
}

bool Database::createTables()
{
    QSqlQuery query(m_db);

    // Users table
    QString createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id TEXT PRIMARY KEY,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createUsersTable)) {
        qCritical() << "Failed to create users table:" << query.lastError().text();
        return false;
    }

    // User data table
    QString createUserDataTable = R"(
        CREATE TABLE IF NOT EXISTS user_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id TEXT NOT NULL,
            data_key TEXT NOT NULL,
            data_value TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id),
            UNIQUE(user_id, data_key)
        )
    )";

    if (!query.exec(createUserDataTable)) {
        qCritical() << "Failed to create user_data table:" << query.lastError().text();
        return false;
    }

    qInfo() << "Database tables created successfully";
    return true;
}

bool Database::createUser(const QString &username, const QString &passwordHash)
{
    QMutexLocker locker(&m_mutex);

    QString userId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO users (id, username, password_hash) VALUES (?, ?, ?)");
    query.addBindValue(userId);
    query.addBindValue(username);
    query.addBindValue(passwordHash);

    if (!query.exec()) {
        qWarning() << "Failed to create user:" << query.lastError().text();
        return false;
    }

    qInfo() << "User created:" << username;
    return true;
}

bool Database::userExists(const QString &username)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }

    return false;
}

QString Database::getUserPasswordHash(const QString &username)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT password_hash FROM users WHERE username = ?");
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

QString Database::getUserId(const QString &username)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM users WHERE username = ?");
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

bool Database::saveUserData(const QString &userId, const QString &dataKey, const QJsonObject &data)
{
    QMutexLocker locker(&m_mutex);

    QJsonDocument doc(data);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO user_data (user_id, data_key, data_value, updated_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        ON CONFLICT(user_id, data_key)
        DO UPDATE SET data_value = ?, updated_at = CURRENT_TIMESTAMP
    )");
    query.addBindValue(userId);
    query.addBindValue(dataKey);
    query.addBindValue(jsonString);
    query.addBindValue(jsonString);

    if (!query.exec()) {
        qWarning() << "Failed to save user data:" << query.lastError().text();
        return false;
    }

    qInfo() << "Data saved for user:" << userId << "key:" << dataKey;
    return true;
}

QJsonObject Database::getUserData(const QString &userId, const QString &dataKey)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("SELECT data_value FROM user_data WHERE user_id = ? AND data_key = ?");
    query.addBindValue(userId);
    query.addBindValue(dataKey);

    if (query.exec() && query.next()) {
        QString jsonString = query.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
        return doc.object();
    }

    return QJsonObject();
}

QJsonArray Database::getAllUserData(const QString &userId)
{
    QMutexLocker locker(&m_mutex);

    QJsonArray dataArray;

    QSqlQuery query(m_db);
    query.prepare("SELECT data_key, data_value, created_at, updated_at FROM user_data WHERE user_id = ?");
    query.addBindValue(userId);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject item;
            item["key"] = query.value(0).toString();

            QString jsonString = query.value(1).toString();
            QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
            item["data"] = doc.object();

            item["created_at"] = query.value(2).toString();
            item["updated_at"] = query.value(3).toString();

            dataArray.append(item);
        }
    }

    return dataArray;
}

bool Database::deleteUserData(const QString &userId, const QString &dataKey)
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM user_data WHERE user_id = ? AND data_key = ?");
    query.addBindValue(userId);
    query.addBindValue(dataKey);

    if (!query.exec()) {
        qWarning() << "Failed to delete user data:" << query.lastError().text();
        return false;
    }

    qInfo() << "Data deleted for user:" << userId << "key:" << dataKey;
    return true;
}
