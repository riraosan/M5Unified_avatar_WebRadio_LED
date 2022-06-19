
/*
 * https://twitter.com/wakwak_koba/
 * modified https://twitter.com/riraosan_0901/
 */

#ifndef _WAKWAK_KOBA_WEBRADIO_JCBASIMUL_HPP_
#define _WAKWAK_KOBA_WEBRADIO_JCBASIMUL_HPP_

#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include "WebRadio.h"

static constexpr const char *station_list[][2] =
    {
        {"Asia Dream", "http://igor.torontocast.com:1025/;.-mp3"},
        {"thejazzstream", "http://wbgo.streamguys.net/thejazzstream"},
        {"Lite Favorites", "http://naxos.cdnstream.com:80/1255_128"},
        {"MAXXED Out", "http://149.56.195.94:8015/steam"},
        {"181-beatles_128k", "http://listen.181fm.com/181-beatles_128k.mp3"},
        {"illstreet-128-mp3", "http://ice1.somafm.com/illstreet-128-mp3"},
        {"bootliquor-128-mp3", "http://ice1.somafm.com/bootliquor-128-mp3"},
        {"dronezone-128-mp3", "http://ice1.somafm.com/dronezone-128-mp3"},
        {"Classic FM", "http://media-ice.musicradio.com:80/ClassicFMMP3"},
};

static byte initial_station = 0;  // Asia Dream

class SimpleWebRadio : public WebRadio {
public:
  SimpleWebRadio(AudioOutput *_out, int cpuDecode, const uint16_t buffSize = 5 * 1024)
      : WebRadio(_out, cpuDecode, 2 * 1024), bufferSize(buffSize) {
    for (int i = 0; i < sizeof(station_list) / sizeof(station_list[0]); i++)
      stations.push_back(new station_t(this, station_list[i][0], station_list[i][1]));
  }

  SimpleWebRadio(AudioOutput *_out, int cpuDecode, uint8_t *_buffer, const uint16_t buffSize = 5 * 1024)
      : WebRadio(_out, cpuDecode, 2 * 1024), buffer(_buffer), bufferSize(buffSize) {
    for (int i = 0; i < sizeof(station_list) / sizeof(station_list[0]); i++)
      stations.push_back(new station_t(this, station_list[i][0], station_list[i][1]));
  }

  ~SimpleWebRadio() {
    if (decoder)
      delete decoder;
    if (source)
      delete source;
  }

  class station_t : public WebRadio::Station {
  public:
    station_t(SimpleWebRadio *_radio, const char *_name, const char *_url) : Station(_radio), name(_name), url(_url) {
    }

    virtual const char *getName() { return name; }
    virtual bool        play() override {
      ((SimpleWebRadio *)radio)->select_station = this;
      return true;
    }

    AudioGeneratorMP3 *getDecoder() {
      auto simpleRadio = (SimpleWebRadio *)radio;
      auto decoder     = new AudioGeneratorMP3();
      decoder->RegisterMetadataCB(simpleRadio->fnCbMetadata, simpleRadio->fnCbMetadata_data);
      decoder->RegisterStatusCB(simpleRadio->fnCbStatus, simpleRadio->fnCbStatus_data);
      return decoder;
    }

    AudioFileSourceBuffer *getSource() {
      auto simpleRadio = (SimpleWebRadio *)radio;

      simpleRadio->stream = new AudioFileSourceICYStream();

      simpleRadio->stream->RegisterMetadataCB(simpleRadio->fnCbMetadata, simpleRadio->fnCbMetadata_data);
      simpleRadio->stream->RegisterStatusCB(simpleRadio->fnCbStatus, simpleRadio->fnCbStatus_data);

      AudioFileSourceBuffer *source;
      if (simpleRadio->buffer)
        source = new AudioFileSourceBuffer(simpleRadio->stream, simpleRadio->buffer, simpleRadio->bufferSize);
      else
        source = new AudioFileSourceBuffer(simpleRadio->stream, simpleRadio->bufferSize);

      if (!simpleRadio->stream->open(url)) {
        delete simpleRadio->stream;
        simpleRadio->stream = nullptr;
      } else {
        auto onChunk = ((SimpleWebRadio *)radio)->onChunk;
        if (onChunk)
          onChunk(url);
      }

      return source;
    }

  private:
    const char *name;
    const char *url;
  };

  virtual bool begin() override {
    return true;
  }

  virtual void decodeTask() override {
    uint64_t saveSettings = 0;
    uint64_t timeout      = 0;

    for (;;) {
      delay(1);

      if (select_station != nullptr) {
        stop();
        current_station = select_station;

        if (onPlay)
          onPlay(current_station->getName(), getIndex(current_station));
        saveSettings = millis() + 10000;

        select_station = nullptr;
      }

      if (current_station && !source)
        source = current_station->getSource();

      if (source && !decoder) {
        decoder = current_station->getDecoder();
        if (!decoder->begin(source, out))
          Serial.println("failed: decoder->begin(source, out)");
      }

      if (decoder && decoder->isRunning()) {
        if (decoder->loop())
          timeout = millis();
        else {
          Serial.println("failed: decoder->loop()");
          stop();
        }
      }

      if (saveSettings > 0 && millis() > saveSettings) {
        saveStation(current_station);
        saveSettings = 0;
      }
    }
  }

  void stop() {
    if (decoder) {
      decoder->stop();
      delete decoder;
      decoder = nullptr;
    }

    if (source) {
      delete source;
      source = nullptr;
    }
  }

  virtual bool play(Station *station = nullptr) override {
    if (!station)
      station = restoreStation();

    if (!station && stations.size() > 0)
      station = stations[initial_station];

    if (station) {
      select_station = (station_t *)station;
      return true;
    }
    return false;
  }

  virtual bool play(bool next) override {
    auto sn = getNumOfStations();
    return play(getStation((getIndex(current_station) + sn + (next ? 1 : -1)) % sn));
  }

  // String getInfoBuffer() {
  //   if (source)
  //     return source->getInfoBuffer();
  //   return "";
  // }

public:
  std::function<void(const char *text)> onChunk = nullptr;

private:
  virtual void saveStationCore(uint32_t nvs_handle, WebRadio::Station *station) override {
    nvs_set_u8(nvs_handle, "WebRadio", getIndex(station));
  }

  virtual WebRadio::Station *restoreStationCore(uint32_t nvs_handle) override {
    uint8_t  idx_8;
    uint32_t idx_32;

    if (nvs_get_u8(nvs_handle, "WebRadio", &idx_8)) {
      if (!nvs_get_u32(nvs_handle, "station", &idx_32)) {
        idx_8 = idx_32;
      } else {
        return nullptr;
      }
    }

    if (idx_8 > getNumOfStations()) {
      idx_8 = getNumOfStations() - 1;
    }

    return getStation(idx_8);
  }

private:
  station_t *select_station  = nullptr;
  station_t *current_station = nullptr;

  AudioFileSourceICYStream *stream  = nullptr;
  AudioFileSourceBuffer    *source  = nullptr;
  AudioGeneratorMP3        *decoder = nullptr;

  uint8_t *buffer = nullptr;
  uint16_t bufferSize;
};

#endif
