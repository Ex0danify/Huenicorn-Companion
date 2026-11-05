#include "api/ApiClient.hpp"

#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>

ApiClient::ApiClient(QObject* parent)
  : QObject(parent)
{
}


void ApiClient::setBaseUrl(const QUrl& url)
{
  m_baseUrl = url;
}


QUrl ApiClient::baseUrl() const
{
  return m_baseUrl;
}


QByteArray ApiClient::encodeBody(const QJsonValue& value)
{
  const auto wrapped = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
  return wrapped.mid(1, wrapped.size() - 2);
}


QNetworkRequest ApiClient::makeRequest(const QString& path, bool json, int timeoutMs) const
{
  QNetworkRequest request(m_baseUrl.resolved(QUrl(path)));
  request.setTransferTimeout(timeoutMs);
  if(json){
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  }
  return request;
}


void ApiClient::get(const QString& path, Callback callback, int timeoutMs)
{
  dispatch(m_network.get(makeRequest(path, false, timeoutMs)), std::move(callback));
}


void ApiClient::put(const QString& path, const QJsonValue& body, Callback callback)
{
  dispatch(m_network.put(makeRequest(path, true), encodeBody(body)), std::move(callback));
}


void ApiClient::post(const QString& path, const QJsonValue& body, Callback callback, int timeoutMs)
{
  dispatch(m_network.post(makeRequest(path, true, timeoutMs), encodeBody(body)), std::move(callback));
}


void ApiClient::dispatch(QNetworkReply* reply, Callback callback)
{
  connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)](){
    reply->deleteLater();
    const bool ok = reply->error() == QNetworkReply::NoError;
    const auto document = ok ? QJsonDocument::fromJson(reply->readAll()) : QJsonDocument();
    if(callback){
      callback(ok, document);
    }
  });
}
