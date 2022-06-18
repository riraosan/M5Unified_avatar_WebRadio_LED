
// 2022-06-18 MIT https://twitter.com/riraosan_0901

// LEDレベルメータを使用する;
//#define USE_FASTLED
#define AVATAR

#define WIFI_SSID "Buffalo-C130"
#define WIFI_PASS "nnkxnpshmhai6"

#include <math.h>
#include <WiFi.h>
#include <nvs.h>

#include "WebRadio_Simple.hpp"

#include <Button2.h>
#define BUTTON_PIN26 26  // Button Unit RED
#define BUTTON_PIN32 32  // Button Unit BLUE
#define BUTTON_PIN39 39  // ATOM Lite Body Push Button

static Button2 bRed;
static Button2 bBlue;
static Button2 Btn;

#include <M5Unified.h>
#include <ESP32_8BIT_CVBS.h>
static ESP32_8BIT_CVBS display;
static M5Canvas        canvas;

static uint32_t lcd_width;
static uint32_t lcd_height;
static int32_t  offset_x;
static int32_t  offset_y;

#include "Avatar.h"

#ifdef USE_FASTLED
#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 10
#if defined(ARDUINO_M5STACK_Core2)
#define DATA_PIN 25
#else
#define DATA_PIN 15
#endif

// Define the array of leds
CRGB leds[NUM_LEDS];
CRGB led_table[NUM_LEDS / 2] = {CRGB::Blue, CRGB::Green, CRGB::Yellow, CRGB::Orange, CRGB::Red};

void turn_off_led() {
  // Now turn the LED off, then pause
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
  FastLED.show();
}

void fill_led_buff(CRGB color) {
  // Now turn the LED off, then pause
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = color;
}

void clear_led_buff() {
  // Now turn the LED off, then pause
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black;
}

void level_led(int level1, int level2) {
  if (level1 > NUM_LEDS / 2) level1 = NUM_LEDS / 2;
  if (level2 > NUM_LEDS / 2) level2 = NUM_LEDS / 2;
  clear_led_buff();
  for (int i = 0; i < level1; i++) {
    leds[NUM_LEDS / 2 - 1 - i] = led_table[i];
  }
  for (int i = 0; i < level2; i++) {
    leds[i + NUM_LEDS / 2] = led_table[i];
  }
  FastLED.show();
}
#else
void turn_off_led() {}
// void fill_led_buff(CRGB color) {}
void clear_led_buff() {}
void level_led(int level1, int level2) {}
#endif

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;

class AudioOutputM5Speaker : public AudioOutput {
public:
  AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0) {
    _m5sound    = m5sound;
    _virtual_ch = virtual_sound_channel;
  }
  virtual ~AudioOutputM5Speaker(void){};
  virtual bool begin(void) override { return true; }
  virtual bool ConsumeSample(int16_t sample[2]) override {
    if (_tri_buffer_index < tri_buf_size) {
      _tri_buffer[_tri_index][_tri_buffer_index]     = sample[0];
      _tri_buffer[_tri_index][_tri_buffer_index + 1] = sample[1];
      _tri_buffer_index += 2;

      return true;
    }

    flush();
    return false;
  }
  virtual void flush(void) override {
    if (_tri_buffer_index) {
      _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch);
      _tri_index        = _tri_index < 2 ? _tri_index + 1 : 0;
      _tri_buffer_index = 0;
      ++_update_count;
    }
  }
  virtual bool stop(void) override {
    flush();
    _m5sound->stop(_virtual_ch);
    for (size_t i = 0; i < 3; ++i) {
      memset(_tri_buffer[i], 0, tri_buf_size * sizeof(int16_t));
    }
    ++_update_count;
    return true;
  }

  const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }
  const uint32_t getUpdateCount(void) const { return _update_count; }

protected:
  m5::Speaker_Class*      _m5sound;
  uint8_t                 _virtual_ch;
  static constexpr size_t tri_buf_size = 640;
  int16_t                 _tri_buffer[3][tri_buf_size];
  size_t                  _tri_buffer_index = 0;
  size_t                  _tri_index        = 0;
  size_t                  _update_count     = 0;
};

#define FFT_SIZE 256
class fft_t {
  float    _wr[FFT_SIZE + 1];
  float    _wi[FFT_SIZE + 1];
  float    _fr[FFT_SIZE + 1];
  float    _fi[FFT_SIZE + 1];
  uint16_t _br[FFT_SIZE + 1];
  size_t   _ie;

public:
  fft_t(void) {
#ifndef M_PI
#define M_PI 3.141592653
#endif
    _ie                          = logf((float)FFT_SIZE) / log(2.0) + 0.5;
    static constexpr float omega = 2.0f * M_PI / FFT_SIZE;
    static constexpr int   s4    = FFT_SIZE / 4;
    static constexpr int   s2    = FFT_SIZE / 2;
    for (int i = 1; i < s4; ++i) {
      float f     = cosf(omega * i);
      _wi[s4 + i] = f;
      _wi[s4 - i] = f;
      _wr[i]      = f;
      _wr[s2 - i] = -f;
    }
    _wi[s4] = _wr[0] = 1;

    size_t je = 1;
    _br[0]    = 0;
    _br[1]    = FFT_SIZE / 2;
    for (size_t i = 0; i < _ie - 1; ++i) {
      _br[je << 1] = _br[je] >> 1;
      je           = je << 1;
      for (size_t j = 1; j < je; ++j) {
        _br[je + j] = _br[je] + _br[j];
      }
    }
  }

  void exec(const int16_t* in) {
    memset(_fi, 0, sizeof(_fi));
    for (size_t j = 0; j < FFT_SIZE / 2; ++j) {
      float  basej = 0.25 * (1.0 - _wr[j]);
      size_t r     = FFT_SIZE - j - 1;

      /// perform han window and stereo to mono convert.
      _fr[_br[j]] = basej * (in[j * 2] + in[j * 2 + 1]);
      _fr[_br[r]] = basej * (in[r * 2] + in[r * 2 + 1]);
    }

    size_t s = 1;
    size_t i = 0;
    do {
      size_t ke = s;
      s <<= 1;
      size_t je = FFT_SIZE / s;
      size_t j  = 0;
      do {
        size_t k = 0;
        do {
          size_t l    = s * j + k;
          size_t m    = ke * (2 * j + 1) + k;
          size_t p    = je * k;
          float  Wxmr = _fr[m] * _wr[p] + _fi[m] * _wi[p];
          float  Wxmi = _fi[m] * _wr[p] - _fr[m] * _wi[p];
          _fr[m]      = _fr[l] - Wxmr;
          _fi[m]      = _fi[l] - Wxmi;
          _fr[l] += Wxmr;
          _fi[l] += Wxmi;
        } while (++k < ke);
      } while (++j < je);
    } while (++i < _ie);
  }

  uint32_t get(size_t index) {
    return (index < FFT_SIZE / 2) ? (uint32_t)sqrtf(_fr[index] * _fr[index] + _fi[index] * _fi[index]) : 0u;
  }
};

static constexpr size_t     WAVE_SIZE = 320;
static AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);
static SimpleWebRadio       radio(&out, PRO_CPU_NUM);
static fft_t                fft;
static bool                 fft_enabled  = false;
static bool                 wave_enabled = false;
static uint16_t             prev_y[(FFT_SIZE / 2) + 1];
static uint16_t             peak_y[(FFT_SIZE / 2) + 1];
static int16_t              wave_y[WAVE_SIZE];
static int16_t              wave_h[WAVE_SIZE];
static int16_t              raw_data[WAVE_SIZE * 2];
static int                  header_height     = 0;
static char                 stream_title[128] = {0};
static const char*          meta_text[2]      = {nullptr, stream_title};
static const size_t         meta_text_num     = sizeof(meta_text) / sizeof(meta_text[0]);
static uint8_t              meta_mod_bits     = 0;

static void MDCallback(void* cbData, const char* type, bool isUnicode, const char* string) {
  (void)cbData;
  if ((strcmp(type, "StreamTitle") == 0) && (strcmp(stream_title, string) != 0)) {
    strncpy(stream_title, string, sizeof(stream_title));
    meta_mod_bits |= 2;
  }
}

static void StCallback(void* cbData, int code, const char* string) {
  Serial.println(string);
}

static uint32_t bgcolor(M5Canvas* gfx, int y) {
  auto h  = gfx->height();
  auto dh = h - header_height;
  int  v  = ((h - y) << 5) / dh;
  if (dh > 44) {
    int v2 = ((h - y - 1) << 5) / dh;
    if ((v >> 2) != (v2 >> 2)) {
      return 0x666666u;
    }
  }
  return gfx->color888(v + 2, v, v + 6);
}

static void gfxSetup(M5Canvas* gfx) {
  if (gfx == nullptr) {
    return;
  }
  if (gfx->width() < gfx->height()) {
    gfx->setRotation(gfx->getRotation() ^ 1);
  }
  gfx->setFont(&fonts::lgfxJapanGothic_12);
  gfx->setTextWrap(false);
  gfx->setCursor(0, 8);
  gfx->print(" WebRadio player");
  gfx->fillRect(0, 6, gfx->width(), 2, TFT_BLACK);

  header_height = (gfx->height() > 80) ? 33 : 21;

  fft_enabled  = true;
  wave_enabled = false;

  for (int y = header_height; y < gfx->height(); ++y) {
    gfx->drawFastHLine(0, y, gfx->width(), bgcolor(gfx, y));
  }

  for (int x = 0; x < (FFT_SIZE / 2) + 1; ++x) {
    prev_y[x] = INT16_MAX;
    peak_y[x] = INT16_MAX;
  }

  for (int x = 0; x < WAVE_SIZE; ++x) {
    wave_y[x] = gfx->height();
    wave_h[x] = 0;
  }
}

void gfxLoop(M5Canvas* gfx) {
  if (gfx == nullptr) {
    return;
  }

  if (header_height > 32) {
    if (meta_mod_bits) {
      for (int id = 0; id < meta_text_num; ++id) {
        if (0 == (meta_mod_bits & (1 << id))) {
          continue;
        }
        meta_mod_bits &= ~(1 << id);
        size_t y = id * 12;
        if (y + 12 >= header_height) {
          continue;
        }
        gfx->setCursor(4, 8 + y);
        gfx->fillRect(0, 8 + y, gfx->width(), 12, gfx->getBaseColor());
        gfx->print(meta_text[id]);
        gfx->print(" ");  // Garbage data removal when UTF8 characters are broken in the middle.
      }
      canvas.pushSprite(&display, offset_x, offset_y);
    }
  } else {
    static int title_x;
    static int title_id;
    static int wait = INT16_MAX;

    if (meta_mod_bits) {
      if (meta_mod_bits & 1) {
        title_x  = 4;
        title_id = 0;
        gfx->fillRect(0, 8, gfx->width(), 12, gfx->getBaseColor());
      }
      meta_mod_bits = 0;
      wait          = 0;
    }

    if (--wait < 0) {
      int tx  = title_x;
      int tid = title_id;
      wait    = 3;

      uint_fast8_t no_data_bits = 0;
      do {
        if (tx == 4) {
          wait = 255;
        }
        gfx->setCursor(tx, 8);
        const char* meta = meta_text[tid];
        if (meta[0] != 0) {
          gfx->print(meta);
          gfx->print("  /  ");
          tx = gfx->getCursorX();
          if (++tid == meta_text_num) {
            tid = 0;
          }
          if (tx <= 4) {
            title_x  = tx;
            title_id = tid;
          }
        } else {
          if ((no_data_bits |= 1 << tid) == ((1 << meta_text_num) - 1)) {
            break;
          }
          if (++tid == meta_text_num) {
            tid = 0;
          }
        }
      } while (tx < gfx->width());
      --title_x;
      canvas.pushSprite(&display, offset_x, offset_y);
    }
  }

  if (fft_enabled) {
    static int prev_x[2];
    static int peak_x[2];

    auto buf = out.getBuffer();
    if (buf) {
      memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t));  // stereo data copy

      int32_t levels[2];
      // draw stereo level meter
      for (size_t i = 0; i < 2; ++i) {
        int32_t level = 0;
        for (size_t j = i; j < 640; j += 32) {
          uint32_t lv = abs(raw_data[j]);
          if (level < lv) {
            level = lv;
          }
        }
        levels[i] = level;

        int32_t x  = (level * gfx->width()) / INT16_MAX;
        int32_t px = prev_x[i];
        if (px != x) {
          gfx->fillRect(x, i * 3, px - x, 2, px < x ? 0xFF9900u : 0x330000u);
          prev_x[i] = x;
        }
        px = peak_x[i];
        if (px > x) {
          gfx->writeFastVLine(px, i * 3, 2, TFT_BLACK);
          px--;
        } else {
          px = x;
        }
        if (peak_x[i] != px) {
          peak_x[i] = px;
          gfx->writeFastVLine(px, i * 3, 2, TFT_WHITE);
        }
      }

      canvas.pushSprite(&display, offset_x, offset_y);

      level_led(levels[0] * 8 / INT16_MAX, levels[1] * 8 / INT16_MAX);

      // draw FFT level meter
      fft.exec(raw_data);
      size_t bw = gfx->width() / 60;
      if (bw < 3) {
        bw = 3;
      }
      int32_t dsp_height = gfx->height();
      int32_t fft_height = dsp_height - header_height - 1;
      size_t  xe         = gfx->width() / bw;
      if (xe > (FFT_SIZE / 2)) {
        xe = (FFT_SIZE / 2);
      }
      // int32_t wave_next = ((header_height + dsp_height) >> 1) + (((256 - (raw_data[0] + raw_data[1])) * fft_height) >> 17);

      uint32_t bar_color[2] = {0x000033u, 0x99AAFFu};

      for (size_t bx = 0; bx <= xe; ++bx) {
        size_t x = bx * bw;
        if ((x & 7) == 0) {
          // canvas.pushSprite(&display, offset_x, offset_y); //NG レベルメーターの描画速度を下げてしまう
          taskYIELD();
        }
        int32_t f = fft.get(bx);
        int32_t y = (f * fft_height) >> 18;
        if (y > fft_height) {
          y = fft_height;
        }
        y          = dsp_height - y;
        int32_t py = prev_y[bx];
        if (y != py) {
          gfx->fillRect(x, y, bw - 1, py - y, bar_color[(y < py)]);
          prev_y[bx] = y;
        }
        py = peak_y[bx] + 1;
        if (py < y) {
          gfx->writeFastHLine(x, py - 1, bw - 1, bgcolor(gfx, py - 1));
        } else {
          py = y - 1;
        }
        if (peak_y[bx] != py) {
          peak_y[bx] = py;
          gfx->writeFastHLine(x, py, bw - 1, TFT_WHITE);
        }
#if defined(WAVE)
        if (wave_enabled) {
          for (size_t bi = 0; bi < bw; ++bi) {
            size_t i = x + bi;
            if (i >= gfx->width() || i >= WAVE_SIZE) {
              break;
            }
            y              = wave_y[i];
            int32_t h      = wave_h[i];
            bool    use_bg = (bi + 1 == bw);
            if (h > 0) {  /// erase previous wave.
              gfx->setAddrWindow(i, y, 1, h);
              h += y;
              do {
                uint32_t bg = (use_bg || y < peak_y[bx]) ? bgcolor(gfx, y)
                              : (y == peak_y[bx])        ? 0xFFFFFFu
                                                         : bar_color[(y >= prev_y[bx])];
                gfx->writeColor(bg, 1);
              } while (++y < h);
            }
            size_t  i2 = i << 1;
            int32_t y1 = wave_next;
            wave_next  = ((header_height + dsp_height) >> 1) + (((256 - (raw_data[i2] + raw_data[i2 + 1])) * fft_height) >> 17);
            int32_t y2 = wave_next;
            if (y1 > y2) {
              int32_t tmp = y1;
              y1          = y2;
              y2          = tmp;
            }
            y         = y1;
            h         = y2 + 1 - y;
            wave_y[i] = y;
            wave_h[i] = h;
            if (h > 0) {  /// draw new wave.
              gfx->setAddrWindow(i, y, 1, h);
              h += y;
              do {
                uint32_t bg = (y < prev_y[bx]) ? 0xFFCC33u : 0xFFFFFFu;
                gfx->writeColor(bg, 1);
              } while (++y < h);
            }
          }
        }
#endif
      }
      canvas.pushSprite(&display, offset_x, offset_y);
    }
  }

  if (!gfx->displayBusy()) {  // draw volume bar
    static int px;
    uint8_t    v = M5.Speaker.getChannelVolume(m5spk_virtual_channel);
    int        x = v * (gfx->width()) >> 8;
    if (px != x) {
      gfx->fillRect(x, 6, px - x, 2, px < x ? 0xAAFFAAu : 0u);
      canvas.pushSprite(&display, offset_x, offset_y);
      px = x;
    }
  }
}

#if defined(AVATAR)

using namespace m5avatar;

Avatar*         avatar;
static M5Canvas sp_avatar(&display);

void lipSync(void* args) {
  float         gazeX, gazeY;
  int           level  = 0;
  DriveContext* ctx    = (DriveContext*)args;
  Avatar*       avatar = ctx->getAvatar();
  for (;;) {
    level = abs(*out.getBuffer());
    if (level < 1500) level = 0;
    if (level > 15000) {
      level = 15000;
    }
    float open = (float)level / 15000.0;
    avatar->setMouthOpenRatio(open);
    avatar->getGaze(&gazeY, &gazeX);
    avatar->setRotation(gazeX * 5);
    delay(50);
  }
}

void setupAvatar(void) {
  avatar = new Avatar(&sp_avatar);

  avatar->setScale(0.65);
  avatar->setOffset(-30, 40);

  ColorPalette cp;
  cp.set(COLOR_PRIMARY, TFT_WHITE);
  cp.set(COLOR_BACKGROUND, TFT_BLACK);
  avatar->setColorPalette(cp);
  avatar->init();  // start drawing
  avatar->addTask(lipSync, "lipSync");
}
#endif

void setupDisplay(void) {
  display.begin();
  display.startWrite();

  if (display.width() < display.height()) {
    display.setRotation(display.getRotation() ^ 1);
  }

  lcd_width  = display.width();
  lcd_height = display.height();

  // offset_x = display.width() - (display.width() >> 2) - 20;
  // offset_y = display.height() - (display.height() >> 2) - 5;
  offset_x = 0;
  offset_y = 8;
}

void setupLevelMeter(void) {
  canvas.setColorDepth(display.getColorDepth());
  canvas.setFont(&fonts::lgfxJapanGothic_12);
  if (canvas.createSprite(display.width(), display.height() >> 2)) {
    canvas.clear();
    gfxSetup(&canvas);
    canvas.pushSprite(&display, offset_x, offset_y);
  } else {
    log_e("can't allocate.");
  }
}

void setupWiFi(void) {
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

#if defined(WIFI_SSID) && defined(WIFI_PASS)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
#else
  WiFi.begin();
#endif

  // canvas.setCursor(0, 8);
  canvas.print(" Connecting");
  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    canvas.print(".");
    canvas.pushSprite(&display, offset_x, offset_y);
    display.display();
    delay(100);
  }

  Serial.print("IP address:");
  Serial.println(WiFi.localIP());
}

void setupAudio(void) {
  auto cfg = M5.config();

  // for ATOMI SPK
  cfg.external_spk = true;  // use external speaker (SPK HAT / ATOMIC SPK)
  M5.begin(cfg);

  {  // custom setting
    auto spk_cfg = M5.Speaker.config();
    //  sample_rateを上げると、CPU負荷が上がる代わりに音質が向上します。
    spk_cfg.sample_rate      = 192000 * 2;  // default:64000 (64kH z)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    spk_cfg.i2s_port         = i2s_port_t::I2S_NUM_1;  // CVBS use IS2_NUM_0. Audio use I2S_NUM_1.
    spk_cfg.pin_bck          = 33;
    spk_cfg.pin_ws           = 22;
    spk_cfg.pin_data_out     = 19;
    spk_cfg.stereo           = true;
    spk_cfg.use_dac          = false;  // about internal DAC

    // for ES9038Q2M VR1.07 DAC Board
    spk_cfg.dma_buf_count   = 40;
    spk_cfg.dma_buf_len     = 64;
    spk_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;  // DACのサンプリングビットが32の方はこちら
    // spk_cfg.bits_per_sample  = I2S_BITS_PER_SAMPLE_16BIT; //DACのサンプリングビットが16の方はこちら

    M5.Speaker.config(spk_cfg);
  }

  {
    uint32_t nvs_handle;
    if (!nvs_open("SimpleWebRadio", NVS_READONLY, &nvs_handle)) {
      size_t volume;
      nvs_get_u32(nvs_handle, "volume", &volume);
      nvs_close(nvs_handle);
      M5.Speaker.setChannelVolume(m5spk_virtual_channel, volume);
    }
  }
}

void setupFastLED(void) {
#ifdef USE_FASTLED
  if (M5.getBoard() == m5::board_t::board_M5Stack) {
    FastLED.addLeds<SK6812, GPIO_NUM_15, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  } else if (M5.getBoard() == m5::board_t::board_M5StackCore2) {
    FastLED.addLeds<SK6812, GPIO_NUM_25, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
  }
  FastLED.setBrightness(32);
  level_led(5, 5);
  FastLED.show();
#endif
}

void setupWebRadio(void) {
  radio.onPlay = [](const char* station_name, const size_t station_idx) {
    Serial.printf("onPlay:%d %s\n", station_idx, station_name);
    meta_text[0]    = station_name;
    stream_title[0] = 0;
    meta_mod_bits   = 3;
  };

  radio.onChunk = [](const char* text) {
    Serial.println(text);
  };

  radio.RegisterMetadataCB(MDCallback, (void*)"ICY");
  radio.RegisterStatusCB(StCallback, (void*)"mp3");
  if (!radio.begin()) {
    Serial.println("failed: radio.begin()");
    for (;;)
      ;
  }
  radio.play();
}

void pressed(Button2& btn) {
  M5.Speaker.tone(440, 100);
}

void doubleClick(Button2& btn) {
  if (btn == bRed) {
    M5.Speaker.tone(1000, 100);
    radio.play(true);
  } else if (btn == bBlue) {
    M5.Speaker.tone(800, 100);
    radio.play(false);
  }
}

void longClick(Button2& btn) {
  display.fillScreen(TFT_BLACK);
  M5.Speaker.stop();
  M5.Speaker.end();
  delay(1000);
  ESP.restart();
}

void click(Button2& btn) {
  size_t v   = M5.Speaker.getChannelVolume(m5spk_virtual_channel);
  int    add = (btn == bRed) ? 5 : -5;
  v += add;

  if (v <= 255) {
    M5.Speaker.setChannelVolume(m5spk_virtual_channel, v);
  }
  if (add > 0) {
    M5.Speaker.tone(1000, 100);
  } else {
    M5.Speaker.tone(800, 100);
  }
}

void tripleClick(Button2& btn) {
  //?
}

void setupButton(void) {
  bRed.setDoubleClickTime(300);
  bRed.setPressedHandler(pressed);
  bRed.setClickHandler(click);
  bRed.setDoubleClickHandler(doubleClick);
  bRed.setTripleClickHandler(tripleClick);
  bRed.begin(BUTTON_PIN26);

  bBlue.setDoubleClickTime(300);
  bBlue.setPressedHandler(pressed);
  bBlue.setClickHandler(click);
  bBlue.setDoubleClickHandler(doubleClick);
  bBlue.setTripleClickHandler(tripleClick);
  bBlue.begin(BUTTON_PIN32);

  Btn.setPressedHandler(pressed);
  Btn.setLongClickHandler(longClick);
  Btn.begin(BUTTON_PIN39);
}

void setup(void) {
  setupButton();
  setupDisplay();
  setupAudio();
  setupLevelMeter();
  setupAvatar();
  // setupFastLED();

  setupWiFi();
  setupWebRadio();

  M5.Speaker.begin();
}

void wait(void) {
  static int prev_frame;
  int        frame;
  do {
    delay(1);
  } while (prev_frame == (frame = millis() >> 3));  /// 8 msec cycle wait
  prev_frame = frame;
}

void loop(void) {
  gfxLoop(&canvas);
  display.display();

  M5.update();
  bRed.loop();
  bBlue.loop();
  Btn.loop();

  wait();
}
