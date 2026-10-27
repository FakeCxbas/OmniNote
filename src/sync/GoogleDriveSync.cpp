#include "GoogleDriveSync.hpp"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <iostream>

namespace OmniNote {

static const char *DEFAULT_CLIENT_ID = "";
static const char *DEFAULT_CLIENT_SECRET = "";

GoogleDriveSyncService::GoogleDriveSyncService(QObject *parent)
    : QObject(parent),
      m_netManager(new QNetworkAccessManager(this)),
      m_loopbackServer(new QTcpServer(this)) {
    connect(m_loopbackServer, &QTcpServer::newConnection, this, &GoogleDriveSyncService::onNewTcpConnection);
    m_clientId = QString::fromUtf8(DEFAULT_CLIENT_ID);
    m_clientSecret = QString::fromUtf8(DEFAULT_CLIENT_SECRET);
    loadCredentials();
}

GoogleDriveSyncService::~GoogleDriveSyncService() {
    cancelAuthorization();
}

QString GoogleDriveSyncService::credentialsFilePath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + "/gdrive_credentials.json";
}

bool GoogleDriveSyncService::isConfigured() const {
    return !m_clientId.trimmed().isEmpty();
}

bool GoogleDriveSyncService::isConnected() const {
    return !m_refreshToken.trimmed().isEmpty();
}

void GoogleDriveSyncService::setCredentials(const QString &clientId, const QString &clientSecret) {
    m_clientId = clientId.trimmed();
    m_clientSecret = clientSecret.trimmed();
    saveCredentials();
}

bool GoogleDriveSyncService::importCredentialsJson(const QString &jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject appObj;
    if (root.contains("installed") && root["installed"].isObject()) {
        appObj = root["installed"].toObject();
    } else if (root.contains("web") && root["web"].isObject()) {
        appObj = root["web"].toObject();
    } else {
        appObj = root;
    }

    QString cid = appObj["client_id"].toString();
    QString sec = appObj["client_secret"].toString();

    if (cid.isEmpty()) {
        return false;
    }

    m_clientId = cid.trimmed();
    m_clientSecret = sec.trimmed();
    saveCredentials();
    return true;
}

bool GoogleDriveSyncService::loadCredentials() {
    QString targetPath = credentialsFilePath();
    if (!QFile::exists(targetPath)) {
        QStringList candidates = {
            QDir::homePath() + "/.config/OmniNote/OmniNote STEM Engine/gdrive_credentials.json",
            QDir::homePath() + "/.config/OmniNote/OmniNoteSTEM/gdrive_credentials.json",
            QDir::homePath() + "/.config/OmniNote/gdrive_credentials.json"
        };
        for (const auto &cand : candidates) {
            if (QFile::exists(cand)) {
                QFile::copy(cand, targetPath);
                break;
            }
        }
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    QString savedCid = obj["client_id"].toString().trimmed();
    if (!savedCid.isEmpty()) {
        m_clientId = savedCid;
    }
    QString savedSecret = obj["client_secret"].toString().trimmed();
    if (!savedSecret.isEmpty()) {
        m_clientSecret = savedSecret;
    }
    m_refreshToken = obj["refresh_token"].toString();
    m_accessToken = obj["access_token"].toString();
    m_userEmail = obj["user_email"].toString();

    if (obj.contains("token_expiry")) {
        m_tokenExpiry = QDateTime::fromString(obj["token_expiry"].toString(), Qt::ISODate);
    }
    if (obj.contains("last_sync_time")) {
        m_lastSyncTime = QDateTime::fromString(obj["last_sync_time"].toString(), Qt::ISODate);
    }

    return true;
}

bool GoogleDriveSyncService::saveCredentials() {
    QJsonObject obj;
    obj["client_id"] = m_clientId;
    obj["client_secret"] = m_clientSecret;
    obj["refresh_token"] = m_refreshToken;
    obj["access_token"] = m_accessToken;
    obj["user_email"] = m_userEmail;
    obj["token_expiry"] = m_tokenExpiry.toString(Qt::ISODate);
    obj["last_sync_time"] = m_lastSyncTime.toString(Qt::ISODate);

    QFile file(credentialsFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

void GoogleDriveSyncService::disconnectAccount() {
    m_refreshToken.clear();
    m_accessToken.clear();
    m_tokenExpiry = QDateTime();
    m_userEmail.clear();
    m_rootFolderId.clear();
    m_folderCache.clear();
    m_fileIdCache.clear();
    saveCredentials();
    emit syncStatusChanged("Cuenta desconectada de Google Drive.");
}

QString GoogleDriveSyncService::generateRandomString(int length) {
    const QString characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    QString result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->bounded(characters.length());
        result.append(characters.at(index));
    }
    return result;
}

QString GoogleDriveSyncService::computeCodeChallenge(const QString &verifier) {
    QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

void GoogleDriveSyncService::startAuthorization() {
    if (!isConfigured()) {
        emit authorizationError("Por favor ingresa o importa un Client ID de Google Cloud antes de conectar.");
        return;
    }

    cancelAuthorization();

    if (!m_loopbackServer->listen(QHostAddress::LocalHost, 0)) {
        emit authorizationError("No se pudo iniciar el servidor local para la autenticación OAuth.");
        return;
    }

    m_isAuthorizing = true;
    int port = m_loopbackServer->serverPort();
    m_codeVerifier = generateRandomString(64);
    QString codeChallenge = computeCodeChallenge(m_codeVerifier);

    QUrl authUrl("https://accounts.google.com/o/oauth2/v2/auth");
    QUrlQuery query;
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("redirect_uri", QString("http://127.0.0.1:%1").arg(port));
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope", "https://www.googleapis.com/auth/drive.file https://www.googleapis.com/auth/userinfo.email");
    query.addQueryItem("code_challenge", codeChallenge);
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("access_type", "offline");
    query.addQueryItem("prompt", "consent");
    authUrl.setQuery(query);

    emit authorizationUrlReady(authUrl.toString());
    QDesktopServices::openUrl(authUrl);
}

void GoogleDriveSyncService::cancelAuthorization() {
    if (m_loopbackServer->isListening()) {
        m_loopbackServer->close();
    }
    m_isAuthorizing = false;
}

void GoogleDriveSyncService::onNewTcpConnection() {
    QTcpSocket *socket = m_loopbackServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        QByteArray requestData = socket->readAll();
        QString requestStr = QString::fromUtf8(requestData);

        // Extract HTTP GET line: "GET /?code=... HTTP/1.1"
        int firstLineEnd = requestStr.indexOf("\r\n");
        QString firstLine = requestStr.left(firstLineEnd);

        int port = m_loopbackServer->serverPort();

        QString code;
        QString error;
        if (firstLine.contains("code=")) {
            int codeStart = firstLine.indexOf("code=") + 5;
            int codeEnd = firstLine.indexOf(' ', codeStart);
            int amp = firstLine.indexOf('&', codeStart);
            if (amp != -1 && amp < codeEnd) codeEnd = amp;
            if (codeEnd == -1) codeEnd = firstLine.length();
            code = QUrl::fromPercentEncoding(firstLine.mid(codeStart, codeEnd - codeStart).toUtf8());
        } else if (firstLine.contains("error=")) {
            int errStart = firstLine.indexOf("error=") + 6;
            int errEnd = firstLine.indexOf(' ', errStart);
            int amp = firstLine.indexOf('&', errStart);
            if (amp != -1 && amp < errEnd) errEnd = amp;
            if (errEnd == -1) errEnd = firstLine.length();
            error = QUrl::fromPercentEncoding(firstLine.mid(errStart, errEnd - errStart).toUtf8());
        }

        // Send HTTP 200 response with beautiful dark-mode Tokyo Night styling
        QString htmlBody;
        if (!code.isEmpty()) {
            htmlBody =
                "<!DOCTYPE html><html><head><meta charset='utf-8'><title>OmniNote STEM Conectado</title></head>"
                "<body style='background:#16161e; color:#c0caf5; font-family:system-ui,-apple-system,sans-serif; text-align:center; padding:60px 20px;'>"
                "  <div style='max-width:480px; margin:0 auto; background:#1a1b26; border:1px solid #3b4261; border-radius:12px; padding:32px; box-shadow:0 8px 24px rgba(0,0,0,0.5);'>"
                "    <h2 style='color:#7aa2f7; margin-top:0; font-size:22px;'>✓ OmniNote STEM Conectado</h2>"
                "    <p style='font-size:15px; color:#a9b1d6; line-height:1.5;'>Tu cuenta de Google Drive ha sido vinculada con éxito.</p>"
                "    <p style='font-size:13px; color:#565f89;'>Ya puedes cerrar esta pestaña del navegador y volver a la aplicación.</p>"
                "  </div>"
                "</body></html>";
        } else {
            htmlBody =
                "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Error de Autenticación</title></head>"
                "<body style='background:#16161e; color:#c0caf5; font-family:system-ui,-apple-system,sans-serif; text-align:center; padding:60px 20px;'>"
                "  <div style='max-width:480px; margin:0 auto; background:#1a1b26; border:1px solid #f7768e; border-radius:12px; padding:32px;'>"
                "    <h2 style='color:#f7768e; margin-top:0;'>✕ Error de Autorización</h2>"
                "    <p style='font-size:14px; color:#c0caf5;'>No se completó la vinculación con Google Drive (" + error + ").</p>"
                "  </div>"
                "</body></html>";
        }

        QByteArray response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "Content-Length: " + QByteArray::number(htmlBody.toUtf8().size()) + "\r\n\r\n" +
            htmlBody.toUtf8();

        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();

        cancelAuthorization();

        if (!code.isEmpty()) {
            exchangeCodeForTokens(code, m_codeVerifier, port);
        } else {
            emit authorizationError("Autorización cancelada o rechazada: " + error);
        }
    });
}

void GoogleDriveSyncService::exchangeCodeForTokens(const QString &code, const QString &codeVerifier, int redirectPort) {
    emit syncStatusChanged("Canjeando código de autorización...");

    QUrl tokenUrl("https://oauth2.googleapis.com/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("client_id", m_clientId);
    if (!m_clientSecret.isEmpty()) {
        postData.addQueryItem("client_secret", m_clientSecret);
    }
    postData.addQueryItem("code", code);
    postData.addQueryItem("code_verifier", codeVerifier);
    postData.addQueryItem("grant_type", "authorization_code");
    postData.addQueryItem("redirect_uri", QString("http://127.0.0.1:%1").arg(redirectPort));

    QNetworkReply *reply = m_netManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit authorizationError(QString("Error al canjear token: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit authorizationError("Respuesta de tokens inválida de Google.");
            return;
        }

        QJsonObject obj = doc.object();
        m_accessToken = obj["access_token"].toString();
        if (obj.contains("refresh_token")) {
            m_refreshToken = obj["refresh_token"].toString();
        }
        int expiresIn = obj["expires_in"].toInt(3600);
        m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn - 60);

        fetchUserInfo([this](bool success, const QString &email) {
            m_userEmail = email;
            saveCredentials();
            if (success) {
                emit authorizationSuccess(email);
            } else {
                emit authorizationSuccess("Conectado");
            }
        });
    });
}

void GoogleDriveSyncService::fetchUserInfo(std::function<void(bool success, const QString &email)> callback) {
    QNetworkRequest request(QUrl("https://www.googleapis.com/oauth2/v2/userinfo"));
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QNetworkReply *reply = m_netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                callback(true, doc.object()["email"].toString());
                return;
            }
        }
        callback(false, "");
    });
}

void GoogleDriveSyncService::ensureValidToken(std::function<void(bool success, const QString &error)> callback) {
    if (!m_accessToken.isEmpty() && m_tokenExpiry.isValid() && m_tokenExpiry > QDateTime::currentDateTime().addSecs(30)) {
        callback(true, "");
        return;
    }

    if (m_refreshToken.isEmpty()) {
        callback(false, "No hay sesión activa. Conéctate a Google Drive.");
        return;
    }

    emit syncStatusChanged("Renovando token de acceso...");

    QUrl tokenUrl("https://oauth2.googleapis.com/token");
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("client_id", m_clientId);
    if (!m_clientSecret.isEmpty()) {
        postData.addQueryItem("client_secret", m_clientSecret);
    }
    postData.addQueryItem("refresh_token", m_refreshToken);
    postData.addQueryItem("grant_type", "refresh_token");

    QNetworkReply *reply = m_netManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(false, QString("Error al refrescar token: %1").arg(reply->errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            callback(false, "Respuesta inválida al renovar token.");
            return;
        }

        QJsonObject obj = doc.object();
        m_accessToken = obj["access_token"].toString();
        int expiresIn = obj["expires_in"].toInt(3600);
        m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn - 60);
        saveCredentials();

        callback(true, "");
    });
}

void GoogleDriveSyncService::checkConnection(std::function<void(bool success, const QString &email, const QString &error)> callback) {
    ensureValidToken([this, callback](bool valid, const QString &err) {
        if (!valid) {
            callback(false, "", err);
            return;
        }
        fetchUserInfo([this, callback](bool success, const QString &email) {
            if (success) {
                m_userEmail = email;
                saveCredentials();
                callback(true, email, "");
            } else {
                callback(true, m_userEmail.isEmpty() ? "Conectado" : m_userEmail, "");
            }
        });
    });
}

void GoogleDriveSyncService::findOrCreateFolder(const QString &folderName, const QString &parentId, std::function<void(bool success, const QString &folderId, const QString &error)> callback) {
    ensureValidToken([this, folderName, parentId, callback](bool valid, const QString &err) {
        if (!valid) {
            callback(false, "", err);
            return;
        }

        QString q = QString("name = '%1' and mimeType = 'application/vnd.google-apps.folder' and trashed = false").arg(folderName);
        if (!parentId.isEmpty()) {
            q += QString(" and '%1' in parents").arg(parentId);
        }

        QUrl listUrl("https://www.googleapis.com/drive/v3/files");
        QUrlQuery query;
        query.addQueryItem("q", q);
        query.addQueryItem("fields", "files(id, name)");
        query.addQueryItem("spaces", "drive");
        listUrl.setQuery(query);

        QNetworkRequest request(listUrl);
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

        QNetworkReply *reply = m_netManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, folderName, parentId, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, "", QString("Error al buscar carpeta: %1").arg(reply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray files = doc.object()["files"].toArray();
            if (!files.isEmpty()) {
                QString existingId = files.first().toObject()["id"].toString();
                callback(true, existingId, "");
                return;
            }

            // Create folder
            QUrl createUrl("https://www.googleapis.com/drive/v3/files");
            QNetworkRequest createReq(createUrl);
            createReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            createReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

            QJsonObject meta;
            meta["name"] = folderName;
            meta["mimeType"] = "application/vnd.google-apps.folder";
            if (!parentId.isEmpty()) {
                QJsonArray parents;
                parents.append(parentId);
                meta["parents"] = parents;
            }

            QNetworkReply *createReply = m_netManager->post(createReq, QJsonDocument(meta).toJson());
            connect(createReply, &QNetworkReply::finished, this, [createReply, callback]() {
                createReply->deleteLater();
                if (createReply->error() != QNetworkReply::NoError) {
                    callback(false, "", QString("Error al crear carpeta: %1").arg(createReply->errorString()));
                    return;
                }
                QJsonDocument cDoc = QJsonDocument::fromJson(createReply->readAll());
                QString newId = cDoc.object()["id"].toString();
                callback(true, newId, "");
            });
        });
    });
}

void GoogleDriveSyncService::uploadFile(const QString &localFilePath, const QString &remoteFileName, const QString &mimeType, const QString &parentFolderId, std::function<void(bool success, const QString &fileId, const QString &error)> callback) {
    ensureValidToken([this, localFilePath, remoteFileName, mimeType, parentFolderId, callback](bool valid, const QString &err) {
        if (!valid) {
            callback(false, "", err);
            return;
        }

        QFile file(localFilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            callback(false, "", "No se pudo leer el archivo local: " + localFilePath);
            return;
        }
        QByteArray fileContent = file.readAll();
        file.close();

        QString cacheKey = QString("%1/%2").arg(parentFolderId, remoteFileName);
        if (m_fileIdCache.contains(cacheKey)) {
            QString cachedId = m_fileIdCache.value(cacheKey);
            QUrl updateUrl(QString("https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media").arg(cachedId));
            QNetworkRequest patchReq(updateUrl);
            patchReq.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
            patchReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

            QNetworkReply *patchReply = m_netManager->sendCustomRequest(patchReq, "PATCH", fileContent);
            connect(patchReply, &QNetworkReply::finished, this, [this, patchReply, cachedId, cacheKey, localFilePath, remoteFileName, mimeType, parentFolderId, callback]() {
                patchReply->deleteLater();
                if (patchReply->error() == QNetworkReply::NoError) {
                    callback(true, cachedId, "");
                } else {
                    // Stale cache entry, clear and re-query
                    m_fileIdCache.remove(cacheKey);
                    uploadFile(localFilePath, remoteFileName, mimeType, parentFolderId, callback);
                }
            });
            return;
        }

        // 1. Check if file already exists in parent folder
        QString q = QString("name = '%1' and trashed = false").arg(remoteFileName);
        if (!parentFolderId.isEmpty()) {
            q += QString(" and '%1' in parents").arg(parentFolderId);
        }

        QUrl listUrl("https://www.googleapis.com/drive/v3/files");
        QUrlQuery query;
        query.addQueryItem("q", q);
        query.addQueryItem("fields", "files(id, name)");
        listUrl.setQuery(query);

        QNetworkRequest listReq(listUrl);
        listReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

        QNetworkReply *listReply = m_netManager->get(listReq);
        connect(listReply, &QNetworkReply::finished, this, [this, listReply, fileContent, remoteFileName, mimeType, parentFolderId, cacheKey, callback]() {
            listReply->deleteLater();
            if (listReply->error() != QNetworkReply::NoError) {
                callback(false, "", QString("Error al consultar archivo: %1").arg(listReply->errorString()));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(listReply->readAll());
            QJsonArray files = doc.object()["files"].toArray();

            if (!files.isEmpty()) {
                // Update existing file in-place (PATCH)
                QString existingId = files.first().toObject()["id"].toString();
                m_fileIdCache[cacheKey] = existingId;
                QUrl updateUrl(QString("https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=media").arg(existingId));
                QNetworkRequest patchReq(updateUrl);
                patchReq.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
                patchReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

                QNetworkReply *patchReply = m_netManager->sendCustomRequest(patchReq, "PATCH", fileContent);
                connect(patchReply, &QNetworkReply::finished, this, [patchReply, existingId, callback]() {
                    patchReply->deleteLater();
                    if (patchReply->error() != QNetworkReply::NoError) {
                        callback(false, "", QString("Error al actualizar archivo: %1").arg(patchReply->errorString()));
                    } else {
                        callback(true, existingId, "");
                    }
                });
            } else {
                // Create new file with multipart upload (POST)
                QUrl uploadUrl("https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
                QNetworkRequest uploadReq(uploadUrl);
                uploadReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

                auto *multiPart = new QHttpMultiPart(QHttpMultiPart::RelatedType);

                // Metadata part
                QHttpPart metaPart;
                metaPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=UTF-8");
                QJsonObject metaObj;
                metaObj["name"] = remoteFileName;
                if (!parentFolderId.isEmpty()) {
                    QJsonArray parents;
                    parents.append(parentFolderId);
                    metaObj["parents"] = parents;
                }
                metaPart.setBody(QJsonDocument(metaObj).toJson());
                multiPart->append(metaPart);

                // File content part
                QHttpPart dataPart;
                dataPart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
                dataPart.setBody(fileContent);
                multiPart->append(dataPart);

                QNetworkReply *uploadReply = m_netManager->post(uploadReq, multiPart);
                multiPart->setParent(uploadReply); // multiPart deleted with reply

                connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply, cacheKey, callback]() {
                    uploadReply->deleteLater();
                    if (uploadReply->error() != QNetworkReply::NoError) {
                        callback(false, "", QString("Error al subir archivo: %1").arg(uploadReply->errorString()));
                        return;
                    }
                    QJsonDocument resDoc = QJsonDocument::fromJson(uploadReply->readAll());
                    QString newId = resDoc.object()["id"].toString();
                    m_fileIdCache[cacheKey] = newId;
                    callback(true, newId, "");
                });
            }
        });
    });
}

void GoogleDriveSyncService::cleanLegacyRootItems(std::function<void(bool success, const QString &message)> callback) {
    if (m_rootFolderId.isEmpty()) {
        callback(true, "");
        return;
    }

    ensureValidToken([this, callback](bool valid, const QString &err) {
        if (!valid) {
            callback(false, err);
            return;
        }

        QString q = QString("'%1' in parents and trashed = false and (name = 'PDFs' or name = 'omninote_backup.db' or name = 'OmniNote_Apuntes.pdf')").arg(m_rootFolderId);

        QUrl listUrl("https://www.googleapis.com/drive/v3/files");
        QUrlQuery query;
        query.addQueryItem("q", q);
        query.addQueryItem("fields", "files(id, name)");
        listUrl.setQuery(query);

        QNetworkRequest listReq(listUrl);
        listReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

        QNetworkReply *reply = m_netManager->get(listReq);
        connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                callback(false, reply->errorString());
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray files = doc.object()["files"].toArray();

            if (files.isEmpty()) {
                callback(true, "");
                return;
            }

            auto trashNext = std::make_shared<std::function<void(int)>>();
            *trashNext = [this, files, trashNext, callback](int idx) {
                if (idx >= files.size()) {
                    callback(true, "Elementos heredados obsoletos limpiados.");
                    return;
                }

                QString fileId = files[idx].toObject()["id"].toString();
                QUrl trashUrl(QString("https://www.googleapis.com/drive/v3/files/%1").arg(fileId));
                QNetworkRequest trashReq(trashUrl);
                trashReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                trashReq.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

                QJsonObject body;
                body["trashed"] = true;

                QNetworkReply *trashReply = m_netManager->sendCustomRequest(trashReq, "PATCH", QJsonDocument(body).toJson());
                connect(trashReply, &QNetworkReply::finished, this, [trashReply, idx, trashNext]() {
                    trashReply->deleteLater();
                    (*trashNext)(idx + 1);
                });
            };

            (*trashNext)(0);
        });
    });
}

void GoogleDriveSyncService::syncDatabase(const QString &dbFilePath, std::function<void(bool success, const QString &message)> callback) {
    if (dbFilePath.trimmed().isEmpty() || !QFile::exists(dbFilePath)) {
        findOrCreateFolder("OmniNote STEM", "", [this, callback](bool fOk, const QString &folderId, const QString &fErr) {
            if (!fOk) {
                callback(false, "Fallo al acceder a carpeta raíz: " + fErr);
                return;
            }
            m_rootFolderId = folderId;
            callback(true, "Carpeta raíz lista.");
        });
        return;
    }

    emit syncProgress(10, "Localizando carpeta 'OmniNote STEM' en Google Drive...");

    findOrCreateFolder("OmniNote STEM", "", [this, dbFilePath, callback](bool fOk, const QString &folderId, const QString &fErr) {
        if (!fOk) {
            callback(false, "Fallo al acceder a carpeta raíz: " + fErr);
            return;
        }

        m_rootFolderId = folderId;
        emit syncProgress(15, "Preparando respaldo de base de datos en .backup/...");

        findOrCreateFolder(".backup", folderId, [this, dbFilePath, callback](bool bOk, const QString &backupFolderId, const QString &bErr) {
            if (!bOk) {
                callback(false, "Fallo al acceder a carpeta de respaldo: " + bErr);
                return;
            }

            uploadFile(dbFilePath, "omninote_backup.db", "application/x-sqlite3", backupFolderId, [this, callback](bool upOk, const QString &fileId, const QString &upErr) {
                Q_UNUSED(fileId);
                if (!upOk) {
                    callback(false, "Fallo al subir base de datos: " + upErr);
                } else {
                    emit syncProgress(25, "Base de datos respaldada en .backup/.");
                    callback(true, "Base de datos respaldada en .backup/.");
                }
            });
        });
    });
}

void GoogleDriveSyncService::syncPdf(const QString &pdfFilePath, const QString &remotePdfName, std::function<void(bool success, const QString &message)> callback) {
    if (!QFile::exists(pdfFilePath)) {
        callback(false, "El archivo PDF no existe: " + pdfFilePath);
        return;
    }

    emit syncProgress(70, "Verificando carpeta 'PDFs' en Drive...");

    findOrCreateFolder("OmniNote STEM", "", [this, pdfFilePath, remotePdfName, callback](bool fOk, const QString &rootId, const QString &fErr) {
        if (!fOk) {
            callback(false, fErr);
            return;
        }

        findOrCreateFolder("PDFs", rootId, [this, pdfFilePath, remotePdfName, callback](bool subOk, const QString &subFolderId, const QString &subErr) {
            if (!subOk) {
                callback(false, subErr);
                return;
            }

            emit syncProgress(85, "Subiendo PDF vectorial a Google Drive...");
            uploadFile(pdfFilePath, remotePdfName, "application/pdf", subFolderId, [callback](bool upOk, const QString &fileId, const QString &upErr) {
                Q_UNUSED(fileId);
                if (!upOk) {
                    callback(false, "Fallo al subir PDF: " + upErr);
                } else {
                    callback(true, "PDF exportado y sincronizado en Google Drive con éxito.");
                }
            });
        });
    });
}

void GoogleDriveSyncService::resolveFolderPath(const QString &folderPath, const QString &parentRootId, std::function<void(bool success, const QString &targetFolderId, const QString &error)> callback) {
    QString cleanPath = folderPath.trimmed();
    while (cleanPath.startsWith('/')) cleanPath.remove(0, 1);
    while (cleanPath.endsWith('/')) cleanPath.chop(1);

    if (cleanPath.isEmpty()) {
        callback(true, parentRootId, "");
        return;
    }

    if (m_folderCache.contains(cleanPath)) {
        callback(true, m_folderCache.value(cleanPath), "");
        return;
    }

    QStringList parts = cleanPath.split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        callback(true, parentRootId, "");
        return;
    }

    auto resolveNext = std::make_shared<std::function<void(int, QString, QString)>>();
    *resolveNext = [this, parts, resolveNext, callback](int index, const QString &currentParentId, const QString &currentPath) {
        if (index >= parts.size()) {
            m_folderCache[currentPath] = currentParentId;
            callback(true, currentParentId, "");
            return;
        }

        QString partName = parts[index];
        QString newPath = currentPath.isEmpty() ? partName : (currentPath + "/" + partName);

        if (m_folderCache.contains(newPath)) {
            (*resolveNext)(index + 1, m_folderCache.value(newPath), newPath);
            return;
        }

        findOrCreateFolder(partName, currentParentId, [this, index, parts, resolveNext, newPath, callback](bool ok, const QString &folderId, const QString &err) {
            if (!ok) {
                callback(false, "", QString("Error al crear carpeta '%1': %2").arg(parts[index], err));
                return;
            }
            m_folderCache[newPath] = folderId;
            (*resolveNext)(index + 1, folderId, newPath);
        });
    };

    (*resolveNext)(0, parentRootId, "");
}

void GoogleDriveSyncService::syncNotebookPages(const QString &dbFilePath,
                                              const QVector<SyncFileEntry> &filesToSync,
                                              std::function<void(bool success, const QString &summary)> callback) {
    emit syncProgress(5, "Iniciando sincronización con Google Drive...");

    findOrCreateFolder("OmniNote STEM", "", [this, dbFilePath, filesToSync, callback](bool fOk, const QString &folderId, const QString &fErr) {
        if (!fOk) {
            emit syncCompleted(false, fErr);
            callback(false, fErr);
            return;
        }
        m_rootFolderId = folderId;

        // Clean legacy items from root so ONLY notebooks appear
        cleanLegacyRootItems([this, dbFilePath, filesToSync, callback](bool /*cOk*/, const QString &/*cMsg*/) {
            syncDatabase(dbFilePath, [this, filesToSync, callback](bool dbOk, const QString &dbMsg) {
                if (!dbOk) {
                    emit syncCompleted(false, dbMsg);
                    callback(false, dbMsg);
                    return;
                }

                if (filesToSync.isEmpty()) {
                    m_lastSyncTime = QDateTime::currentDateTime();
                    saveCredentials();
                    emit syncProgress(100, "¡Sincronización completa!");
                    emit syncCompleted(true, "Sincronización finalizada.");
                    callback(true, "Sincronización finalizada.");
                    return;
                }

                emit syncProgress(30, QString("Preparando carpetas de cuadernos para %1 archivos...").arg(filesToSync.size()));

                const int totalFiles = filesToSync.size();
                auto uploadNext = std::make_shared<std::function<void(int, int)>>();

                *uploadNext = [this, filesToSync, totalFiles, uploadNext, callback](int index, int successCount) {
                    if (index >= totalFiles) {
                        m_lastSyncTime = QDateTime::currentDateTime();
                        saveCredentials();
                        emit syncProgress(100, "¡Sincronización de páginas completa!");
                        QString summary = QString("%1 archivos de cuadernos sincronizados en Google Drive.").arg(successCount);
                        emit syncCompleted(true, summary);
                        callback(true, summary);
                        return;
                    }

                    const auto &item = filesToSync[index];
                    int pct = 30 + static_cast<int>(68.0 * (index + 1) / totalFiles);
                    emit syncProgress(pct, QString("Subiendo %1/%2 (%3 de %4)...").arg(item.subfolderPath, item.remoteFileName).arg(index + 1).arg(totalFiles));

                    resolveFolderPath(item.subfolderPath, m_rootFolderId, [this, item, index, totalFiles, successCount, uploadNext](bool folderOk, const QString &folderId, const QString &folderErr) {
                        if (!folderOk) {
                            emit syncProgress(30 + static_cast<int>(68.0 * (index + 1) / totalFiles),
                                               QString("Advertencia: No se pudo resolver carpeta para %1: %2").arg(item.remoteFileName, folderErr));
                            (*uploadNext)(index + 1, successCount);
                            return;
                        }

                        uploadFile(item.localFilePath, item.remoteFileName, item.mimeType, folderId, [this, index, successCount, uploadNext](bool upOk, const QString &fileId, const QString &upErr) {
                            Q_UNUSED(fileId);
                            Q_UNUSED(upErr);
                            (*uploadNext)(index + 1, successCount + (upOk ? 1 : 0));
                        });
                    });
                };

                (*uploadNext)(0, 0);
            });
        });
    });
}

void GoogleDriveSyncService::syncAll(const QString &dbFilePath, const QString &pdfFilePath, const QString &pdfName, std::function<void(bool success, const QString &summary)> callback) {
    QVector<SyncFileEntry> files;
    if (!pdfFilePath.isEmpty() && QFile::exists(pdfFilePath)) {
        SyncFileEntry entry;
        entry.localFilePath = pdfFilePath;
        entry.remoteFileName = pdfName.isEmpty() ? "OmniNote_Apuntes.pdf" : pdfName;
        entry.subfolderPath = "PDFs";
        entry.mimeType = "application/pdf";
        files.append(entry);
    }
    syncNotebookPages(dbFilePath, files, callback);
}

void GoogleDriveSyncService::syncSingleFile(const SyncFileEntry &fileEntry, std::function<void(bool success, const QString &error)> callback) {
    if (!isConnected()) {
        if (callback) callback(false, "No conectado a Google Drive");
        return;
    }

    ensureValidToken([this, fileEntry, callback](bool valid, const QString &err) {
        if (!valid) {
            if (callback) callback(false, err);
            return;
        }

        findOrCreateFolder("OmniNote STEM", "", [this, fileEntry, callback](bool fOk, const QString &rootFolderId, const QString &fErr) {
            if (!fOk) {
                if (callback) callback(false, fErr);
                return;
            }
            m_rootFolderId = rootFolderId;

            resolveFolderPath(fileEntry.subfolderPath, m_rootFolderId, [this, fileEntry, callback](bool folderOk, const QString &targetFolderId, const QString &folderErr) {
                if (!folderOk) {
                    if (callback) callback(false, folderErr);
                    return;
                }

                uploadFile(fileEntry.localFilePath, fileEntry.remoteFileName, fileEntry.mimeType, targetFolderId, [this, callback](bool upOk, const QString &fileId, const QString &upErr) {
                    Q_UNUSED(fileId);
                    if (upOk) {
                        m_lastSyncTime = QDateTime::currentDateTime();
                        saveCredentials();
                    }
                    if (callback) callback(upOk, upErr);
                });
            });
        });
    });
}

} // namespace OmniNote
