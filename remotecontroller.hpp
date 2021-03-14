#pragma once
#include "api.hpp"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusAbstractAdaptor>


class Mpris2Root : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
public:
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(bool CanSetFullscreen READ canSetFullscreen)
    Q_PROPERTY(bool Fullscreen READ fullscreen WRITE setFullscreen)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

    explicit Mpris2Root(QObject* parent = nullptr);

    bool canRaise();
    bool canQuit();
    bool hasTrackList();
    bool canSetFullscreen();
    bool fullscreen();
    void setFullscreen(bool value);
    QString identity();
    QString desktopEntry();
    QStringList supportedUriSchemes();
    QStringList supportedMimeTypes();

public slots:
    void Raise();
    void Quit();
};

class Mpris2Player : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
public:
    explicit Mpris2Player(QObject* parent = nullptr);

    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(bool CanControl READ canControl)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanStope READ canStop)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(int MinimuRate READ minimumRate)
    Q_PROPERTY(int MaximuRate READ maximumRate)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)

    QString playbackStatus();
    QString loopStatus();
    void setLoopStatus(const QString& status);
    bool shuffle();
    void setShuffle(bool value);
    double volume();
    void setVolume(double value);
    QVariantMap metadata();
    double minimumRate();
    double maximumRate();
    double rate();
    void setRate(float value);
    qint64 position();
    bool canGoNext();
    bool canGoPrevious();
    bool canPlay();
    bool canStop();
    bool canPause();
    bool canSeek();
    bool canControl();

public slots:
    void PlayPause();
    void Play();
    void Pause();
    void Stop();
    void Next();
    void Previous();
    void Seek(qint64 position);
    void SetPosition(const QDBusObjectPath& trackId, qlonglong position);

signals:
    void Seeked(qint64 position);

private slots:
    void onPlaybackStatusChanged();
    void onSongChanged(Track* track);
    void onFavoriteChanged();
    void onArtUrlChanged();
    void onPositionChanged();
    void onDurationChanged();
    void onCanSeekChanged();
    void onCanGoPreviousChanged();
    void onCanGoNextChanged();
    void onVolumeChanged();

private:
    void signalPlayerUpdate(const QVariantMap& map);
    void signalUpdate(const QVariantMap& map, const QString& interfaceName);

    QString qMapToString(const QMap<QString, QVariant>& map);
};

class RemoteController : public QObject
{
  Q_OBJECT
public:
  ~RemoteController();
  explicit RemoteController(QObject *parent = nullptr);

  inline static const QString serviceName = "org.mpris.MediaPlayer2.DTeam.DMusic";

  static QMap<QString, QVariant> toXesam(Track& track);

public slots:

signals:
  void play();
  void pause();
  void playPause();
  void stop();
  void next();
  void previous();
  void seek(qint64 position);

private:

  inline static bool _isDBusServiceCreated = false;
  Mpris2Root* _mpris2Root;
  Mpris2Player* _mpris2Player;
};

