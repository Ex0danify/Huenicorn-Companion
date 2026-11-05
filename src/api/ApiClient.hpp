#pragma once

#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <functional>

// Small async JSON client for the daemon REST API
class ApiClient : public QObject
{
  Q_OBJECT

public:
  static constexpr int DefaultTimeoutMs = 5000;

  using Callback = std::function<void(bool ok, const QJsonDocument& response)>;

  explicit ApiClient(QObject* parent = nullptr);

  void setBaseUrl(const QUrl& url);
  QUrl baseUrl() const;

  void get(const QString& path, Callback callback, int timeoutMs = DefaultTimeoutMs);
  void put(const QString& path, const QJsonValue& body, Callback callback, int timeoutMs = DefaultTimeoutMs);
  void post(const QString& path, const QJsonValue& body, Callback callback, int timeoutMs = DefaultTimeoutMs);

  // The daemon expects bare JSON scalars as bodies, which QJsonDocument cannot hold
  static QByteArray encodeBody(const QJsonValue& value);

private:
  void dispatch(QNetworkReply* reply, Callback callback);
  QNetworkRequest makeRequest(const QString& path, bool json, int timeoutMs = DefaultTimeoutMs) const;

  QNetworkAccessManager m_network;
  QUrl m_baseUrl;
};
