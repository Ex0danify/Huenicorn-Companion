#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

// In-process stand-in for the huenicorn REST API with just enough state to test clients
class FakeDaemonApi : public QObject
{
public:
  struct Request
  {
    QString method;
    QString path;
    QByteArray body;
  };

  explicit FakeDaemonApi(quint16 port = 0)
  {
    resetChannels();
    connect(&m_server, &QTcpServer::newConnection, this, [this](){
      while(auto* socket = m_server.nextPendingConnection()){
        connect(socket, &QTcpSocket::readyRead, this, [this, socket](){ onReadyRead(socket); });
      }
    });
    m_server.listen(QHostAddress::LocalHost, port);
  }

  quint16 port() const { return m_server.serverPort(); }
  void close() { m_server.close(); }

  QList<Request> requests;
  int stopRequests{0};
  bool failSaves{false};

  // Setup mode mimics a daemon without a bridge: a reduced API, everything else is a 404
  bool setupMode{false};
  QStringList detectedBridges{"192.168.1.20"};
  QString knownBridge{"192.168.1.20"};
  QString knownUser{"known-user"};
  QString pairError{"link button not pressed"};
  int linkButtonAfter{0};         // register attempts that fail before the button counts as pressed
  int normalModeAfter{0};         // webUIStatus requests answered with 404 once setup finished
  int registerAttempts{0};
  bool setupFinished{false};
  // The real daemon crashes on a request that lands while its setup server stops
  int shutdownWindowMs{0};
  bool requestedDuringShutdown{false};
  bool versionPolledDuringShutdown{false};

  int count(const QString& method, const QString& pathPrefix) const
  {
    int n = 0;
    for(const auto& r : requests){
      n += (r.method == method && r.path.startsWith(pathPrefix)) ? 1 : 0;
    }
    return n;
  }

  // Body of the most recent request to this path, empty if none
  QByteArray lastBody(const QString& path) const
  {
    for(auto it = requests.crbegin(); it != requests.crend(); ++it){
      if(it->path == path){
        return it->body;
      }
    }
    return {};
  }

  QJsonArray channels;

private:
  void resetChannels()
  {
    auto channel = [](int id, const QString& name, bool active, double x0, double y0, double x1, double y1){
      return QJsonObject{
        {"channelId", id},
        {"active", active},
        {"gammaFactor", 0.0},
        {"uvs", QJsonObject{
          {"uvA", QJsonObject{{"x", x0}, {"y", y0}}},
          {"uvB", QJsonObject{{"x", x1}, {"y", y1}}}
        }},
        {"devices", QJsonArray{QJsonObject{{"id", "d"}, {"name", name}}}}
      };
    };
    channels = {
      channel(0, "Ceiling Spot 1", true, 0.0, 0.0, 0.5, 0.3),
      channel(1, "Cupboard", false, 0.6, 0.5, 0.9, 0.9)
    };
  }

  QJsonObject* findChannel(int id)
  {
    for(int i = 0; i < channels.size(); i++){
      if(channels[i].toObject().value("channelId").toInt() == id){
        m_scratch = channels[i].toObject();
        m_scratchIndex = i;
        return &m_scratch;
      }
    }
    return nullptr;
  }

  void commitScratch() { channels[m_scratchIndex] = m_scratch; }

  void onReadyRead(QTcpSocket* socket)
  {
    auto& buffer = m_buffers[socket];
    buffer.append(socket->readAll());

    const auto headerEnd = buffer.indexOf("\r\n\r\n");
    if(headerEnd < 0){
      return;
    }

    const auto header = QString::fromUtf8(buffer.left(headerEnd));
    int length = 0;
    for(const auto& line : header.split("\r\n")){
      if(line.startsWith("content-length:", Qt::CaseInsensitive)){
        length = line.mid(15).trimmed().toInt();
      }
    }
    if(buffer.size() < headerEnd + 4 + length){
      return;
    }

    const auto parts = header.split("\r\n").first().split(' ');
    Request request{parts.value(0), parts.value(1), buffer.mid(headerEnd + 4, length)};
    m_buffers.remove(socket);
    requests.append(request);
    if(m_sinceFinish.isValid() && m_sinceFinish.elapsed() < shutdownWindowMs){
      (request.path == "/api/version" ? versionPolledDuringShutdown : requestedDuringShutdown) = true;
    }

    m_notFound = false;
    const auto body = route(request);
    const bool fail = failSaves && request.path == "/api/saveProfile";
    const char* status = m_notFound ? "HTTP/1.1 404 Not Found" : (fail ? "HTTP/1.1 500 Internal Server Error" : "HTTP/1.1 200 OK");
    socket->write(QByteArray(status) + "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
      + QByteArray::number(body.size()) + "\r\n\r\n" + body);
    socket->disconnectFromHost();
  }

  static QByteArray json(const QJsonValue& value)
  {
    if(value.isArray()){
      return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact);
    }
    return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
  }

  QByteArray route(const Request& r)
  {
    const auto& p = r.path;
    const auto bodyDoc = QJsonDocument::fromJson("[" + r.body + "]").array().first();

    if(setupMode){
      return routeSetup(r, bodyDoc);
    }
    if(p == "/api/webUIStatus" && normalModeAfter > 0){
      normalModeAfter--;
      m_notFound = true;
      return "{}";
    }

    if(p == "/api/version"){
      return json(QJsonObject{{"version", "9.9.9"}});
    }
    if(p == "/api/stop"){
      stopRequests++;
      return json(QJsonObject{{"succeeded", true}});
    }
    if(p == "/api/entertainmentConfigurations"){
      return json(QJsonObject{
        {"entertainmentConfigurations", QJsonArray{
          QJsonObject{{"entertainmentConfigurationId", "area-a"}, {"name", "Gaming desk"}},
          QJsonObject{{"entertainmentConfigurationId", "area-b"}, {"name", "Living room"}}
        }},
        {"currentEntertainmentConfigurationId", m_area}
      });
    }
    if(p == "/api/channels"){
      return json(channels);
    }
    if(p == "/api/displayInfo"){
      return json(QJsonObject{
        {"x", 2560}, {"y", 1440}, {"subsampleWidth", 256},
        {"subsampleResolutionCandidates", QJsonArray{
          QJsonObject{{"x", 128}, {"y", 72}}, QJsonObject{{"x", 256}, {"y", 144}}, QJsonObject{{"x", 512}, {"y", 288}}
        }},
        {"selectedRefreshRate", 60}, {"maxRefreshRate", 144}, {"selectedTransitionSmoothing", 15.0}
      });
    }
    if(p == "/api/interpolationInfo"){
      return json(QJsonObject{
        {"available", QJsonArray{QJsonObject{{"Nearest", 0}}, QJsonObject{{"Cubic", 1}}, QJsonObject{{"Area", 2}}}},
        {"current", 2}
      });
    }
    if(p == "/api/setEntertainmentConfiguration"){
      m_area = bodyDoc.toString();
      return json(QJsonObject{{"succeeded", true}, {"entertainmentConfigurationId", m_area}, {"channels", channels}});
    }
    if(p.startsWith("/api/setChannelActivity/")){
      if(auto* ch = findChannel(p.section('/', -1).toInt())){
        ch->insert("active", bodyDoc.toObject().value("active").toBool());
        commitScratch();
      }
      return json(QJsonObject{{"succeeded", true}, {"channels", channels}});
    }
    if(p.startsWith("/api/setChannelUV/")){
      if(auto* ch = findChannel(p.section('/', -1).toInt())){
        const auto in = bodyDoc.toObject();
        auto uvs = ch->value("uvs").toObject();
        auto a = uvs.value("uvA").toObject();
        auto b = uvs.value("uvB").toObject();
        const QJsonObject point{{"x", in.value("x")}, {"y", in.value("y")}};
        if(in.value("type").toInt() == 0){ a = point; } else { b = point; }
        uvs = QJsonObject{{"uvA", a}, {"uvB", b}};
        ch->insert("uvs", uvs);
        commitScratch();
        return json(uvs);
      }
    }
    if(p.startsWith("/api/setChannelGammaFactor/")){
      if(auto* ch = findChannel(p.section('/', -1).toInt())){
        ch->insert("gammaFactor", bodyDoc.toObject().value("gammaFactor"));
        commitScratch();
      }
      return json(QJsonObject{{"succeeded", true}});
    }
    if(p == "/api/setSubsampleWidth"){
      return json(QJsonObject{{"x", 2560}, {"y", 1440}, {"subsampleWidth", bodyDoc.toInt()}});
    }
    if(p == "/api/setRefreshRate"){
      return json(QJsonObject{{"refreshRate", bodyDoc.toInt()}});
    }
    if(p == "/api/setInterpolation"){
      return json(QJsonObject{{"interpolation", bodyDoc.toInt()}});
    }
    if(p == "/api/setTransitionSmoothing"){
      return json(QJsonObject{{"transitionSmoothing", bodyDoc.toDouble()}});
    }
    return json(QJsonObject{{"succeeded", true}});
  }

  QByteArray routeSetup(const Request& r, const QJsonValue& bodyDoc)
  {
    const auto& p = r.path;

    if(p == "/api/version"){
      return json(QJsonObject{{"version", "9.9.9"}});
    }
    if(p == "/api/configFilePath"){
      return json(QJsonObject{{"configFilePath", "/tmp/huenicorn/config.json"}});
    }
    if(p == "/api/autodetectBridge"){
      QJsonArray bridges;
      for(const auto& address : detectedBridges){
        bridges.append(QJsonObject{{"internalipaddress", address}});
      }
      return json(QJsonObject{{"succeeded", true}, {"bridges", bridges}});
    }
    if(p == "/api/validateBridgeAddress"){
      m_bridge = bodyDoc.toObject().value("bridgeAddress").toString();
      return json(QJsonObject{{"succeeded", m_bridge == knownBridge}});
    }
    if(p == "/api/registerNewUser"){
      if(++registerAttempts <= linkButtonAfter){
        return json(QJsonObject{{"succeeded", false}, {"error", pairError}});
      }
      return json(QJsonObject{{"succeeded", true}, {"username", "new-user"}, {"clientkey", "00112233445566778899AABBCCDDEEFF"}});
    }
    if(p == "/api/validateCredentials"){
      return json(QJsonObject{{"succeeded", bodyDoc.toObject().value("username").toString() == knownUser}});
    }
    if(p == "/api/abort"){
      return "{}";
    }
    if(p == "/api/finishSetup"){
      setupFinished = true;
      setupMode = false;
      m_sinceFinish.start();
      return json(QJsonObject{{"version", "9.9.9"}});
    }

    m_notFound = true;
    return "{}";
  }

  QTcpServer m_server;
  QHash<QTcpSocket*, QByteArray> m_buffers;
  bool m_notFound{false};
  QElapsedTimer m_sinceFinish;
  QString m_bridge;
  QString m_area{"area-a"};
  QJsonObject m_scratch;
  int m_scratchIndex{0};
};
