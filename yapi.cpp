#include "yapi.hpp"
#include "file.hpp"
#include "settings.hpp"
#include "utils.hpp"
#include "Log.hpp"
#include <thread>
#include <functional>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QMediaPlayer>

using namespace py;

object repeat_if_error(std::function<object()> f, int n = 10, std::string s = "NetworkError") {
  if (s == "") {
    while (true) {
      try {
        return f();
      }  catch (py_error& e) {
        --n;
        if (n <= 0) throw std::move(e);
      }
    }
  } else {
    while (true) {
      try {
        return f();
      }  catch (py_error& e) {
        if (e.type != s) throw std::move(e);
        --n;
        if (n <= 0) throw std::move(e);
      }
    }
  }
  return nullptr;
}

void repeat_if_error(std::function<void()> f, std::function<void(bool success)> r, int n = 10, std::string s = "NetworkError") {
  int tries = n;
  if (s == "") {
    while (true) {
      try {
        f();
        r(true);
        return;
      }  catch (py_error&) {
        --tries;
        if (tries <= 0) {
          r(false);
          return;
        }
      }
    }
  } else {
    while (true) {
      try {
        f();
        r(true);
        return;
      }  catch (py_error& e) {
        if (e.type != s) {
          r(false);
          return;
        }
        --tries;
        if (tries <= 0) {
          r(false);
          return;
        }
      }
    }
  }
}

void do_async(std::function<void()> f) {
  std::thread(f).detach();
}

void repeat_if_error_async(std::function<void()> f, std::function<void(bool success)> r, int n = 10, std::string s = "NetworkError") {
  do_async([=]() {
    int tries = n;
    if (s == "") {
      while (true) {
        try {
          f();
          r(true);
          return;
        }  catch (py_error&) {
          --tries;
          if (tries <= 0) {
            r(false);
            return;
          }
        }
      }
    } else {
      while (true) {
        try {
          f();
          r(true);
          return;
        }  catch (py_error& e) {
          if (e.type != s) {
            r(false);
            return;
          }
          --tries;
          if (tries <= 0) {
            r(false);
            return;
          }
        }
      }
    }
  });
}


YClient::YClient(QObject *parent) : QObject(parent), ym("yandex_music", true), ym_request(ym/"utils"/"request")
{

}

YClient::YClient(const YClient& copy) : QObject(copy.parent()), ym(copy.ym), ym_request(copy.ym_request), me(copy.me), loggined((bool)copy.loggined)
{

}

YClient& YClient::operator=(const YClient& copy)
{
  ym = copy.ym;
  ym_request = copy.ym_request;
  me = copy.me;
  loggined = (bool)copy.loggined;
  return *this;
}

YClient::~YClient()
{
}

bool YClient::isLoggined()
{
  return loggined;
}

QString YClient::token(QString login, QString password)
{
  return ym.call("generate_token_by_username_and_password", {login, password}).to<QString>();
}

bool YClient::login(QString token)
{
  loggined = false;
  repeat_if_error([this, token]() {
    me = ym.call("Client", token);
  }, [this](bool success) {
    loggined = success;
  }, Settings::ym_repeatsIfError());
  return loggined;
}

void YClient::login(QString token, const QJSValue& callback)
{
  do_async<bool>(this, callback, &YClient::login, token);
}

bool YClient::loginViaProxy(QString token, QString proxy)
{
  loggined = false;
  repeat_if_error([this, token, proxy]() {
    std::map<std::string, object> kwargs;
    kwargs["proxy_url"] = proxy;
    object req = ym_request.call("Request", std::initializer_list<object>{}, kwargs);
    kwargs.clear();
    kwargs["request"] = req;
    me = ym.call("Client", token, kwargs);
  }, [this](bool success) {
    loggined = success;
  }, Settings::ym_repeatsIfError());
  return loggined;
}

void YClient::loginViaProxy(QString token, QString proxy, const QJSValue& callback)
{
  do_async<bool>(this, callback, &YClient::loginViaProxy, token, proxy);
}

QVector<object> YClient::fetchTracks(qint64 id)
{
  QVector<py::object> tracks;
  repeat_if_error([this, id, &tracks]() {
    tracks = me.call("tracks", std::vector<object>{id}).to<QVector<py::object>>();
  }, [](bool) {
  }, Settings::ym_repeatsIfError());
  return tracks;
}

std::pair<bool, QList<YTrack*>> YClient::fetchYTracks(qint64 id)
{
  QList<YTrack*> tracks;
  bool successed;
  repeat_if_error([this, id, &tracks]() {
    tracks = me.call("tracks", std::vector<object>{id}).to<QList<YTrack*>>(); // утечка памяти?
    for (auto&& track : tracks) {
      track->_client = this;
      track->setParent(this);
    }
  }, [&successed](bool success) {
    successed = success;
  }, Settings::ym_repeatsIfError());
  move_to_thread(tracks, QGuiApplication::instance()->thread());
  return {successed, tracks};
}

void YClient::fetchYTracks(qint64 id, const QJSValue& callback)
{
  do_async<bool, QList<YTrack*>>(this, callback, &YClient::fetchYTracks, id);
}

Playlist* YClient::likedTracks()
{
  auto a = me.call("users_likes_tracks").get("tracks_ids");
  DPlaylist* res = new DPlaylist(this);
  for (int i = 0; i < PyList_Size(a.raw); ++i) {
    object p = PyList_GetItem(a.raw, i);
    if (!p.contains(":")) continue;
    res->add(new YTrack(p.call("split", ":")[0].to<int>(), this));
  }
  return res;
}

Playlist* YClient::playlist(int id)
{
  if (id == 3) return likedTracks();
  auto a = me.call("playlists_list", me.get("me").get("account").get("uid").to<QString>() + ":" + QString::number(id))[0].call("fetch_tracks");
  DPlaylist* res = new DPlaylist(this);
  for (int i = 0; i < PyList_Size(a.raw); ++i) {
    object p = PyList_GetItem(a.raw, i);
    if (!p.has("id")) continue;
    res->add(new YTrack(p.get("id").to<int>(), this));
  }
  return res;
}

Playlist* YClient::track(qint64 id)
{
  DPlaylist* res = new DPlaylist(this);
  res->add(new YTrack(id, this));
  return res;
}

Playlist* YClient::downloadsPlaylist()
{
  DPlaylist* res = new DPlaylist(this);
  QDir recoredDir(Settings::ym_savePath());
  QStringList allFiles = recoredDir.entryList(QDir::Files, QDir::SortFlag::Name);
  for (auto s : allFiles) {
    if (!s.endsWith(".json")) continue;
    s.chop(5);
    res->add(new YTrack(s.toInt(), this));
  }
  return res;
}


YTrack::YTrack(qint64 id, YClient* client) : Track((QObject*)client)
{
  _id = id;
  _client = client;
}

YTrack::YTrack(object obj, YClient* client) : Track((QObject*)client)
{
  _id = obj.get("id").to<qint64>();
  _client = client;
}

YTrack::~YTrack()
{

}

YTrack::YTrack(QObject* parent) : Track(parent)
{

}

QString YTrack::idInt()
{
  return QString::number(_id);
}

QString YTrack::title()
{
  if (!_checkedDisk) _loadFromDisk();
  if (_title.isEmpty() && !_noTitle) {
    do_async([this](){ _fetchYandex(); saveMetadata(); });
  }
  return _title;
}

QString YTrack::author()
{
  if (!_checkedDisk) _loadFromDisk();
  if (_author.isEmpty() && !_noAuthor) {
    do_async([this](){ _fetchYandex(); saveMetadata(); });
  }
  return _author;
}

QString YTrack::extra()
{
  if (!_checkedDisk) _loadFromDisk();
  if (_extra.isEmpty() && !_noExtra) {
    do_async([this](){ _fetchYandex(); saveMetadata(); });
  }
  return _extra;
}

QString YTrack::cover()
{
  if (!_checkedDisk) _loadFromDisk();
  if (Settings::ym_saveCover()) {
    if (_cover.isEmpty()) {
      if (!_noCover) _downloadCover(); // async
      else emit coverAborted();
      return "qrc:resources/player/no-cover.svg";
    }
    auto s = _relativePathToCover? QDir::cleanPath(Settings::ym_savePath() + QDir::separator() + _cover) : _cover;
    if (!fileExists(s)) {
      if (_relativePathToCover)
        _downloadCover(); // async
      return "qrc:resources/player/no-cover.svg";
    }
    return "file:" + s;
  } else {
    if (_py == none) do_async([this](){
      _fetchYandex();
      saveMetadata();
      if (!_py.has("cover_uri")) return;
      emit coverChanged(_coverUrl());
    });
    else
      return _coverUrl();
  }
  return "qrc:resources/player/no-cover.svg";
}

QMediaContent YTrack::media()
{
  if (!_checkedDisk) _loadFromDisk();
  if (Settings::ym_downloadMedia()) {
    if (_media.isEmpty()) {
      if (!_noMedia) _downloadMedia(); // async
      else {
        logging.warning("yandex/" + QString::number(id()) + ": no media");
        emit mediaAborted();
      }
      return {};
    }
    auto media = QDir::cleanPath(Settings::ym_savePath() + QDir::separator() + _media);
    if (QFile::exists(media))
      return QMediaContent("file:" + media);
    emit mediaAborted();
  } else {
    if (_py == none) do_async([this](){
      _fetchYandex();
      saveMetadata();
      emit mediaChanged(QMediaContent(QUrl(_py.call("get_download_info")[0].call("get_direct_link").to<QString>())));
    });
    else
      return QMediaContent(QUrl(_py.call("get_download_info")[0].call("get_direct_link").to<QString>()));
  }
  return {};
}

qint64 YTrack::duration()
{
  if (!_checkedDisk) _loadFromDisk();
  if (_duration == 0 && !_noMedia) {
    do_async([this](){ _fetchYandex(); saveMetadata(); });
  }
  return _duration;
}

bool YTrack::liked()
{
  if (!_checkedDisk) _loadFromDisk();
  if (!_hasLiked) {
    do_async([this](){ _fetchYandex(); saveMetadata(); });
  }
  return _liked;
}

qint64 YTrack::id()
{
  return _id;
}

bool YTrack::available()
{
  //TODO: check _py is not nil
  return _py.get("available").to<bool>();
}

QVector<YArtist> YTrack::artists()
{
  //TODO: check _py is not nil
  return _py.get("artists").to<QVector<YArtist>>();
}

QString YTrack::coverPath()
{
  return Settings::ym_coverPath(id());
}

QString YTrack::metadataPath()
{
  return Settings::ym_metadataPath(id());
}

QString YTrack::mediaPath()
{
  return Settings::ym_mediaPath(id());
}

QJsonObject YTrack::jsonMetadata()
{
  QJsonObject info;
  info["hasTitle"] = !_noTitle;
  info["hasAuthor"] = !_noAuthor;
  info["hasExtra"] = !_noExtra;
  info["hasCover"] = !_noCover;
  info["hasMedia"] = !_noMedia;

  info["title"] = _title;
  info["extra"] = _extra;
  info["artistsNames"] = _author;
  info["artists"] = toQJsonArray(_artists);
  info["cover"] = _cover;
  info["relativePathToCover"] = _relativePathToCover;
  info["duration"] = _duration;
  if (_hasLiked) info["liked"] = _liked;
  return info;
}

QString YTrack::stringMetadata()
{
  auto json = QJsonDocument(jsonMetadata()).toJson(QJsonDocument::Compact);
  return json.data();
}

void YTrack::saveMetadata()
{
  if (!Settings::ym_saveInfo()) return;
  if (_id <= 0) return;
  File(metadataPath()).writeAll(jsonMetadata());
}

void YTrack::setLiked(bool liked)
{
  do_async([this, liked](){
    QMutexLocker lock(&_mtx);
    _fetchYandex();
    repeat_if_error([this, liked]() {
      if (liked) {
        _py.call("like");
      } else {
        _py.call("dislike");
      }
      _liked = liked;
      emit likedChanged(liked);
    }, [](bool) {}, Settings::ym_repeatsIfError());
    saveMetadata();
  });
}

bool YTrack::_loadFromDisk()
{
  _checkedDisk = true;
  if (!Settings::ym_saveInfo()) return false;
  if (_id <= 0) return false;
  auto metadataPath = Settings::ym_metadataPath(_id);
  if (!fileExists(metadataPath)) return false;

  QJsonObject doc = File(metadataPath).allJson().object();

  _noTitle = !doc["hasTitle"].toBool(true);
  _noAuthor = !doc["hasAuthor"].toBool(true);
  _noExtra = !doc["hasExtra"].toBool(true);
  _noCover = !doc["hasCover"].toBool(true);
  _noMedia = !doc["hasMedia"].toBool(true);

  _title = doc["title"].toString("");
  _author = doc["artistsNames"].toString("");
  _extra = doc["extra"].toString("");
  _cover = doc["cover"].toString("");
  _relativePathToCover = doc["relativePathToCover"].toBool(true);
  _media = _noMedia? "" : QString::number(_id) + ".mp3";

  auto liked = doc["liked"];
  if (liked.isBool()) _hasLiked = true;
  _liked = liked.toBool(false);

  if (!doc["duration"].isDouble() && !_noMedia) {
    //TODO: load duration from media and save data to file. use taglib
    _duration = 0;
    do_async([this](){ _fetchYandex(); saveMetadata(); });
  } else {
    _duration = doc["duration"].toInt();
  }

  return true;
}

void YTrack::_fetchYandex()
{
  QMutexLocker lock(&_mtx);
  if (_py != py::none) return;
  auto _pys = _client->fetchTracks(_id);
  if (_pys.empty()) {
    _fetchYandex(none);
  } else {
    _fetchYandex(_pys[0]);
  }
}

void YTrack::_fetchYandex(object _pys)
{
  QMutexLocker lock(&_mtx);
  if (_pys == none) {
    _title = "";
    _author = "";
    _extra = "";
    _cover = "";
    _media = "";
    _duration = 0;
    _noTitle = true;
    _noAuthor = true;
    _noExtra = true;
    _noCover = true;
    _noMedia = true;
    _liked = false;
  } else {
    _py = _pys;

    _title = _py.get("title").to<QString>();
    _noTitle = _title.isEmpty();
    emit titleChanged(_title);

    auto artists_py = _py.get("artists").to<QVector<py::object>>();
    QVector<QString> artists_str;
    artists_str.reserve(artists_py.length());
    for (auto&& e : artists_py) {
      artists_str.append(e.get("name").to<QString>());
      _artists.append(e.get("id").to<qint64>());
    }
    _author = join(artists_str, ", ");
    _noAuthor = _author.isEmpty();
    emit authorChanged(_author);

    _extra = _py.get("version").to<QString>();
    _noExtra = _extra.isEmpty();
    emit extraChanged(_extra);

    _duration = _py.get("duration_ms").to<qint64>();
    emit durationChanged(_duration);

    //TODO: отдельно фетчить liked
    auto ult = _py.get("client").call("users_likes_tracks").get("tracks_ids");
    _liked = false;
    for (int i = 0; i < PyList_Size(ult.raw); ++i) {
      object p = PyList_GetItem(ult.raw, i);
      if (!p.contains(":")) continue;
      if (p.call("split", ":")[0].to<int>() == _id) {
        _liked = true;
        break;
      }
    }
    emit likedChanged(_liked);
  }
}

void YTrack::_downloadCover()
{
  do_async([this](){
    QMutexLocker lock(&_mtx);
    _fetchYandex();
    if (_noCover) {
      _cover = "";
      emit coverAborted();
      return;
    }
    repeat_if_error([this]() {
      _py.call("download_cover", std::initializer_list<object>{coverPath(), Settings::ym_coverQuality()});
      _cover = QString::number(_id) + ".png";
    }, [this](bool success) {
      if (success) emit coverChanged(cover());
      else {
        _noCover = true;
        emit coverAborted();
      }
    }, Settings::ym_repeatsIfError());
    saveMetadata();
  });
}

void YTrack::_downloadMedia()
{
  do_async([this](){
    QMutexLocker lock(&_mtx);
    _fetchYandex();
    if (_noMedia) {
      emit mediaAborted();
      return;
    }
    repeat_if_error([this]() {
      _py.call("download", mediaPath());
      _media = QString::number(_id) + ".mp3";
    }, [this](bool success) {
      if (success) emit mediaChanged(media());
      else {
        _noCover = true;
        emit mediaAborted();
      }
    }, Settings::ym_repeatsIfError());
    saveMetadata();
  });
}

QString YTrack::_coverUrl()
{
  if (!_py.has("cover_uri")) return "";
  auto a = "https://" + _py.get("cover_uri").to<QString>();
  a.remove(a.length() - 2, 2);
  a += "m" + Settings::ym_coverQuality();
  return a;
}


YArtist::YArtist(object impl, QObject* parent) : QObject(parent)
{
  this->impl = impl;
  _id = impl.get("id").to<int>();
}

YArtist::YArtist()
{

}

YArtist::YArtist(const YArtist& copy) : QObject(nullptr), impl(copy.impl), _id(copy._id)
{

}

YArtist& YArtist::operator=(const YArtist& copy)
{
  impl = copy.impl;
  _id = copy._id;
  return *this;
}

int YArtist::id()
{
  return _id;
}

QString YArtist::name()
{
  return impl.get("name").to<QString>();
}

QString YArtist::coverPath()
{
  return Settings::ym_artistCoverPath(id());
}

QString YArtist::metadataPath()
{
  return Settings::ym_artistMetadataPath(id());
}

QJsonObject YArtist::jsonMetadata()
{
  QJsonObject info;
  info["id"] = id();
  info["name"] = name();
  info["cover"] = coverPath();
  return info;
}

QString YArtist::stringMetadata()
{
  auto json = QJsonDocument(jsonMetadata()).toJson(QJsonDocument::Compact);
  return json.data();
}

void YArtist::saveMetadata()
{
  File(metadataPath()).writeAll(jsonMetadata());
}

bool YArtist::saveCover(int quality)
{
  bool successed;
  QString size = QString::number(quality) + "x" + QString::number(quality);
  repeat_if_error([this, size]() {
    impl.call("download_og_image", std::initializer_list<object>{coverPath(), size});
  }, [&successed](bool success) {
    successed = success;
  }, Settings::ym_repeatsIfError());
  return successed;
}

void YArtist::saveCover(int quality, const QJSValue& callback)
{
  do_async<bool>(this, callback, &YArtist::saveCover, quality);
}
