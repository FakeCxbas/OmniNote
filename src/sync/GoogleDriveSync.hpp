#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;

namespace OmniNote {

class GoogleDriveSyncService : public QObject {
    Q_OBJECT
public:
    explicit GoogleDriveSyncService(QObject *parent = nullptr);
    ~GoogleDriveSyncService() override;

    // Credentials & Connection state
    bool isConfigured() const;
    bool isConnected() const;
    QString userEmail() const { return m_userEmail; }
    QString clientId() const { return m_clientId; }
    QString clientSecret() const { return m_clientSecret; }
    QDateTime lastSyncTime() const { return m_lastSyncTime; }

    void setCredentials(const QString &clientId, const QString &clientSecret);
    bool importCredentialsJson(const QString &jsonFilePath);
    bool loadCredentials();
    bool saveCredentials();
    void disconnectAccount();

    // OAuth 2.0 PKCE Authorization
    void startAuthorization();
    void cancelAuthorization();

    // Batch & Page-by-Page Sync Structures
    struct SyncFileEntry {
        QString localFilePath;
        QString remoteFileName;   // e.g. "Página 1.pdf"
        QString subfolderPath;    // e.g. "PDFs/Apuntes" or "PDFs"
        QString mimeType;         // e.g. "application/pdf"
    };

    // Drive Operations
    void checkConnection(std::function<void(bool success, const QString &email, const QString &error)> callback);
    void findOrCreateFolder(const QString &folderName, const QString &parentId, std::function<void(bool success, const QString &folderId, const QString &error)> callback);
    void resolveFolderPath(const QString &folderPath, const QString &parentRootId, std::function<void(bool success, const QString &targetFolderId, const QString &error)> callback);
    void uploadFile(const QString &localFilePath, const QString &remoteFileName, const QString &mimeType, const QString &parentFolderId, std::function<void(bool success, const QString &fileId, const QString &error)> callback);
    void cleanLegacyRootItems(std::function<void(bool success, const QString &message)> callback);
    void syncDatabase(const QString &dbFilePath, std::function<void(bool success, const QString &message)> callback);
    void syncPdf(const QString &pdfFilePath, const QString &remotePdfName, std::function<void(bool success, const QString &message)> callback);
    void syncAll(const QString &dbFilePath, const QString &pdfFilePath, const QString &pdfName, std::function<void(bool success, const QString &summary)> callback);
    void syncNotebookPages(const QString &dbFilePath, const QVector<SyncFileEntry> &filesToSync, std::function<void(bool success, const QString &summary)> callback);

signals:
    void authorizationUrlReady(const QString &url);
    void authorizationSuccess(const QString &userEmail);
    void authorizationError(const QString &errorMessage);
    void syncProgress(int percentage, const QString &message);
    void syncStatusChanged(const QString &statusText);
    void syncCompleted(bool success, const QString &summaryMessage);

private slots:
    void onNewTcpConnection();

private:
    void exchangeCodeForTokens(const QString &code, const QString &codeVerifier, int redirectPort);
    void ensureValidToken(std::function<void(bool success, const QString &error)> callback);
    void fetchUserInfo(std::function<void(bool success, const QString &email)> callback);

    static QString generateRandomString(int length);
    static QString computeCodeChallenge(const QString &verifier);
    static QString credentialsFilePath();

    QNetworkAccessManager *m_netManager{nullptr};
    QTcpServer *m_loopbackServer{nullptr};

    QString m_clientId;
    QString m_clientSecret;
    QString m_codeVerifier;

    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_tokenExpiry;
    QString m_userEmail;
    QDateTime m_lastSyncTime;

    QString m_rootFolderId;
    QMap<QString, QString> m_folderCache;
    bool m_isAuthorizing{false};
};

} // namespace OmniNote
