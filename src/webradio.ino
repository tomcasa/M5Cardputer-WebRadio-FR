
/**
 * @file M5Cardputer_WebRadio.ino
 * @author Aurélio Avanzi Dutch version Roland Breedveld
 * @brief https://github.com/cyberwisk/M5Cardputer_WebRadio
 * @version Beta 1.2
 * @date 2024-05-25
 *
 * @Hardwares: M5Cardputer
 * @Platform Version: Arduino M5Stack Board Manager v2.0.7
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 * ESP8266Audio: https://github.com/earlephilhower/ESP8266Audio/
 * M5Cardputer: https://github.com/m5stack/M5Cardputer
 * Preferences: https://github.com/espressif/arduino-esp32/tree/master/libraries/Preferences
 **/

#include <M5Cardputer.h>
#include <M5Unified.h>
#include <Preferences.h>
#include "CardWifiSetup.h"

#include <AudioOutput.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>

#include <HTTPClient.h>
#include <vector>  

#define PIN_LED    21
#define NUM_LEDS   1

int PreviousStation = 0;
int previousVolume = 0;

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;

/// set web radio station url

std::vector<std::pair<String, String>> stationList;


void getStationList() {
  HTTPClient http;
  http.begin("http://philae.synology.me/~admin/arduino/radio_dico.php");
  /*
  expected format 
Vibration PopRock;http://vibrationpoprock.ice.infomaniak.ch/vibrationpoprock-high.mp3
Vibration SoftHits;http://vibration.stream2net.eu/vibra-softhits.mp3
Vivacité BXL;http://redbeemedia.streamabc.net/redbm-vivabxl-mp3-160-6153830
La Première BXL;http://redbeemedia.streamabc.net/redbm-lapremiere-mp3-160-7134387
Radio Contact;http://radiocontact.ice.infomaniak.ch/radiocontact-mp3-192.mp3
Couleur 3;http://stream.srg-ssr.ch/m/couleur3/mp3_128
France Inter;http://icecast.radiofrance.fr/franceinter-midfi.mp3
France Culture;http://icecast.radiofrance.fr/franceculture-midfi.mp3
France Musique;http://icecast.radiofrance.fr/francemusique-midfi.mp3
Tarmac;http://redbeemedia.streamabc.net/redbm-tarmac-mp3-160-3948377
Tipik;http://redbeemedia.streamabc.net/redbm-tipik-mp3-160-1909468
Sud Radio FR;http://start-sud.ice.infomaniak.ch/start-sud-high.mp3
Grand Grenoble;http://alpes1grenoble.ice.infomaniak.ch/alpes1grenoble-high.mp3
France Bleu Nord;http://icecast.radiofrance.fr/fbnord-midfi.mp3
France Bleu Occitanie;http://icecast.radiofrance.fr/fbtoulouse-midfi.mp3
FIP;http://icecast.radiofrance.fr/fip-midfi.mp3
Skyrock;http://icecast.skyrock.net/s/natio_mp3_128k
France Info;http://icecast.radiofrance.fr/franceinfo-midfi.mp3
Campus BXL;http://streamer.radiocampus.be:8000/stream.mp3
Europe 1;http://stream.europe1.fr/europe1.mp3
FUN Radio;http://icecast.funradio.fr/fun-1-44-128?listen=webCwsBCggNCQgLDQUGBAcGBg
Le Mouv;http://direct.mouv.fr/live/mouv-midfi.mp3
Antipode Brabant Wallon;http://streaming.antipode.be/antipode.mp3
100% Toulouse;http://100radio-toulouse.ice.infomaniak.ch/100radio-toulouse-128.mp3

  
  */
  
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    M5Cardputer.Display.println("List received !");
    delay(1000); 
    parseStations(payload);
  } else {
    M5Cardputer.Display.print("Erreur HTTP : ");
    M5Cardputer.Display.println(httpCode);
  }


  http.end();


}

void parseStations(const String& data) {
  stationList.clear();

  int start = 0;
  while (start < data.length()) {
    int end = data.indexOf('\n', start);
    if (end == -1) end = data.length();

    String line = data.substring(start, end);
    int sep = line.indexOf(';');
    if (sep != -1) {
      String name = line.substring(0, sep);
      String url = line.substring(sep + 1);
      name.trim();
      name = String(stationList.size()) + " - "+ name; 
      url.trim();
      if (name.length() && url.length())
        stationList.push_back({ name, url });
    }

    start = end + 1;
  }
}


/*

const int MAX_STATIONS = 70; 
char station_list[MAX_STATIONS][2][100]; // 70 stations, 2 champs, 100 caractères max


static constexpr const char* station_list[][2] =
{
  
  {"Philae 128k Local"      , "http://192.168.0.13:8012/master_mixx"},
  {"Philae 128k Ext"        , "https://philae.synology.me/master_mixx"},
  {"Philae 320k Ext"        , "https://philae.synology.me/mixx_mp3_320"}

};*/

//constexpr const size_t stations = sizeof(station_list) / sizeof(station_list[0]);

class AudioOutputM5Speaker : public AudioOutput
{
  public:
    AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0)
    {
      _m5sound = m5sound;
      _virtual_ch = virtual_sound_channel;
    }
    virtual ~AudioOutputM5Speaker(void) {};
    virtual bool begin(void) override { return true; }
    virtual bool ConsumeSample(int16_t sample[2]) override
    {
      if (_tri_buffer_index < tri_buf_size)
      {
        _tri_buffer[_tri_index][_tri_buffer_index  ] = sample[0];
        _tri_buffer[_tri_index][_tri_buffer_index+1] = sample[1];
        _tri_buffer_index += 2;

        return true;
      }

      flush();
      return false;
    }
    virtual void flush(void) override
    {
      if (_tri_buffer_index)
      {
        _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch);
        _tri_index = _tri_index < 2 ? _tri_index + 1 : 0;
        _tri_buffer_index = 0;
        ++_update_count;
      }
    }
    virtual bool stop(void) override
    {
      flush();
      _m5sound->stop(_virtual_ch);
      for (size_t i = 0; i < 3; ++i)
      {
        memset(_tri_buffer[i], 0, tri_buf_size * sizeof(int16_t));
      }
      ++_update_count;
      return true;
    }

    const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }
    const uint32_t getUpdateCount(void) const { return _update_count; }

  protected:
    m5::Speaker_Class* _m5sound;
    uint8_t _virtual_ch;
    static constexpr size_t tri_buf_size = 640;
    int16_t _tri_buffer[3][tri_buf_size];
    size_t _tri_buffer_index = 0;
    size_t _tri_index = 0;
    size_t _update_count = 0;
};


#define FFT_SIZE 256
class fft_t
{
  float _wr[FFT_SIZE + 1];
  float _wi[FFT_SIZE + 1];
  float _fr[FFT_SIZE + 1];
  float _fi[FFT_SIZE + 1];
  uint16_t _br[FFT_SIZE + 1];
  size_t _ie;

public:
  fft_t(void)
  {
#ifndef M_PI
#define M_PI 3.141592653
#endif
    _ie = logf( (float)FFT_SIZE ) / log(2.0) + 0.5;
    static constexpr float omega = 2.0f * M_PI / FFT_SIZE;
    static constexpr int s4 = FFT_SIZE / 4;
    static constexpr int s2 = FFT_SIZE / 2;
    for ( int i = 1 ; i < s4 ; ++i)
    {
    float f = cosf(omega * i);
      _wi[s4 + i] = f;
      _wi[s4 - i] = f;
      _wr[     i] = f;
      _wr[s2 - i] = -f;
    }
    _wi[s4] = _wr[0] = 1;

    size_t je = 1;
    _br[0] = 0;
    _br[1] = FFT_SIZE / 2;
    for ( size_t i = 0 ; i < _ie - 1 ; ++i )
    {
      _br[ je << 1 ] = _br[ je ] >> 1;
      je = je << 1;
      for ( size_t j = 1 ; j < je ; ++j )
      {
        _br[je + j] = _br[je] + _br[j];
      }
    }
  }

  void exec(const int16_t* in)
  {
    memset(_fi, 0, sizeof(_fi));
    for ( size_t j = 0 ; j < FFT_SIZE / 2 ; ++j )
    {
      float basej = 0.25 * (1.0-_wr[j]);
      size_t r = FFT_SIZE - j - 1;

      /// perform han window and stereo to mono convert.
      _fr[_br[j]] = basej * (in[j * 2] + in[j * 2 + 1]);
      _fr[_br[r]] = basej * (in[r * 2] + in[r * 2 + 1]);
    }

    size_t s = 1;
    size_t i = 0;
    do
    {
      size_t ke = s;
      s <<= 1;
      size_t je = FFT_SIZE / s;
      size_t j = 0;
      do
      {
        size_t k = 0;
        do
        {
          size_t l = s * j + k;
          size_t m = ke * (2 * j + 1) + k;
          size_t p = je * k;
          float Wxmr = _fr[m] * _wr[p] + _fi[m] * _wi[p];
          float Wxmi = _fi[m] * _wr[p] - _fr[m] * _wi[p];
          _fr[m] = _fr[l] - Wxmr;
          _fi[m] = _fi[l] - Wxmi;
          _fr[l] += Wxmr;
          _fi[l] += Wxmi;
        } while ( ++k < ke) ;
      } while ( ++j < je );
    } while ( ++i < _ie );
  }

  uint32_t get(size_t index)
  {
    return (index < FFT_SIZE / 2) ? (uint32_t)sqrtf(_fr[ index ] * _fr[ index ] + _fi[ index ] * _fi[ index ]) : 0u;
  }
};

static constexpr const int preallocateBufferSize = 128 * 1024;
static constexpr const int preallocateCodecSize = 85332; // MP3 and AAC+SBR codec max mem needed
static void* preallocateBuffer = nullptr;
static void* preallocateCodec = nullptr;
static constexpr size_t WAVE_SIZE = 320;
static AudioOutputM5Speaker out(&M5Cardputer.Speaker, m5spk_virtual_channel);
static AudioGenerator *decoder = nullptr;
static AudioFileSourceICYStream *file = nullptr;
static AudioFileSourceBuffer *buff = nullptr;
static fft_t fft;
static bool fft_enabled = false;
static bool wave_enabled = false;
static uint16_t prev_y[(FFT_SIZE / 2)+1];
static uint16_t peak_y[(FFT_SIZE / 2)+1];
static int16_t wave_y[WAVE_SIZE];
static int16_t wave_h[WAVE_SIZE];
static int16_t raw_data[WAVE_SIZE * 2];
static int header_height = 0;
static size_t station_index = 0;
static char stream_title[128] = { 0 };
static const char* meta_text[2] = { nullptr, stream_title };
static const size_t meta_text_num = sizeof(meta_text) / sizeof(meta_text[0]);
static uint8_t meta_mod_bits = 0;
static volatile size_t playindex = ~0u;

static void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  (void)cbData;
  if ((strcmp(type, "StreamTitle") == 0) && (strcmp(stream_title, string) != 0))
  {
    strncpy(stream_title, string, sizeof(stream_title));
    meta_mod_bits |= 2;
  }
}

static void stop(void)
{
  if (decoder) {
    decoder->stop();
    delete decoder;
    decoder = nullptr;
  }

  if (buff) {
    buff->close();
    delete buff;
    buff = nullptr;
  }
  if (file) {
    file->close();
    delete file;
    file = nullptr;
  }
  out.stop();
}

static void play(size_t index)
{
  playindex = index;
}

static void decodeTask(void*)
{
  for (;;)
  {
    delay(1);
    if (playindex != ~0u)
    {
      auto index = playindex;
      playindex = ~0u;
      stop();
      meta_text[0] = stationList.at(index).first.c_str();
      stream_title[0] = 0;
      meta_mod_bits = 3;
      file = new AudioFileSourceICYStream(stationList.at(index).second.c_str());
      file->RegisterMetadataCB(MDCallback, (void*)"ICY");
      buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
      //decoder = isAAC ? (AudioGenerator*) new AudioGeneratorAAC(preallocateCodec, preallocateCodecSize) : (AudioGenerator*) new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
      decoder = new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
      decoder->begin(buff, &out);
    }
    if (decoder && decoder->isRunning())
    {
      if (!decoder->loop()) { decoder->stop(); }
    }
  }
}

static uint32_t bgcolor(LGFX_Device* gfx, int y)
{
  auto h = gfx->height();
  auto dh = h - header_height;
  int v = ((h - y)<<5) / dh;
  if (dh > 44)
  {
    int v2 = ((h - y - 1)<<5) / dh;
    if ((v >> 2) != (v2 >> 2))
    {
      return 0x666666u;
    }
  }
  return gfx->color888(v + 2, v, v + 6);
}

static void gfxSetup(LGFX_Device* gfx)
{
  if (gfx == nullptr) { return; }
  //if (gfx->width() < gfx->height())
  //{
  //  gfx->setRotation(gfx->getRotation()^1);
  //}
  gfx->setFont(&fonts::lgfxJapanGothic_12);
  //gfx->setTextFont(&fonts::FreeMonoOblique18pt7b);
  gfx->setEpdMode(epd_mode_t::epd_fastest);
  gfx->setTextWrap(false);
  gfx->setCursor(0, 8);
  gfx->println("WebRadio player");
  gfx->fillRect(0, 6, gfx->width(), 2, TFT_BLACK);

  header_height = (gfx->height() > 100) ? 33 : 21;
  fft_enabled = !gfx->isEPD();
  if (fft_enabled)
  {
    wave_enabled = (gfx->getBoard() != m5gfx::board_M5UnitLCD);

    for (int y = header_height; y < gfx->height(); ++y)
    {
      gfx->drawFastHLine(0, y, gfx->width(), bgcolor(gfx, y));
    }
  }

  for (int x = 0; x < (FFT_SIZE/2)+1; ++x)
  {
    prev_y[x] = INT16_MAX;
    peak_y[x] = INT16_MAX;
  }
  for (int x = 0; x < WAVE_SIZE; ++x)
  {
    wave_y[x] = gfx->height();
    wave_h[x] = 0;
  }
}

void gfxLoop(LGFX_Device* gfx)
{
  if (gfx == nullptr) { return; }
  if (header_height > 32)
  {
    if (meta_mod_bits)
    {
      gfx->startWrite();
      for (int id = 0; id < meta_text_num; ++id)
      {
        if (0 == (meta_mod_bits & (1<<id))) { continue; }
        meta_mod_bits &= ~(1<<id);
        size_t y = id * 12;
        if (y+12 >= header_height) { continue; }
        gfx->setCursor(4, 8 + y);
        gfx->fillRect(0, 8 + y, gfx->width(), 12, gfx->getBaseColor());
        gfx->print(meta_text[id]);
        gfx->print(" ");
      }
      gfx->display();
      gfx->endWrite();
    }
  }
  else
  {
    static int title_x;
    static int title_id;
    static int wait = INT16_MAX;

    if (meta_mod_bits)
    {
      if (meta_mod_bits & 1)
      {
        title_x = 4;
        title_id = 0;
        gfx->fillRect(0, 8, gfx->width(), 12, gfx->getBaseColor());
      }
      meta_mod_bits = 0;
      wait = 0;
    }

    if (--wait < 0)
    {
      int tx = title_x;
      int tid = title_id;
      wait = 3;
      gfx->startWrite();
      uint_fast8_t no_data_bits = 0;
      do
      {
        if (tx == 4) { wait = 255; }
        gfx->setCursor(tx, 8);
        const char* meta = meta_text[tid];
        if (meta[0] != 0)
        {
          gfx->print(meta);
          gfx->print("  /  ");
          tx = gfx->getCursorX();
          if (++tid == meta_text_num) { tid = 0; }
          if (tx <= 4)
          {
            title_x = tx;
            title_id = tid;
          }
        }
        else
        {
          if ((no_data_bits |= 1 << tid) == ((1 << meta_text_num) - 1))
          {
            break;
          }
          if (++tid == meta_text_num) { tid = 0; }
        }
      } while (tx < gfx->width());
      --title_x;
      gfx->display();
      gfx->endWrite();
    }
  }

  if (fft_enabled)
  {
    static int prev_x[2];
    static int peak_x[2];

    auto buf = out.getBuffer();
    if (buf)
    {
      memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t)); // stereo data copy
      gfx->startWrite();

      // draw stereo level meter
      for (size_t i = 0; i < 2; ++i)
      {
        int32_t level = 0;
        for (size_t j = i; j < 640; j += 32)
        {
          uint32_t lv = abs(raw_data[j]);
          if (level < lv) { level = lv; }
        }

        int32_t x = (level * gfx->width()) / INT16_MAX;
        int32_t px = prev_x[i];
        if (px != x)
        {
          gfx->fillRect(x, i * 3, px - x, 2, px < x ? 0xFF9900u : 0x330000u);
          prev_x[i] = x;
        }
        px = peak_x[i];
        if (px > x)
        {
          gfx->writeFastVLine(px, i * 3, 2, TFT_BLACK);
          px--;
        }
        else
        {
          px = x;
        }
        if (peak_x[i] != px)
        {
          peak_x[i] = px;
          gfx->writeFastVLine(px, i * 3, 2, TFT_WHITE);
        }
      }
      gfx->display();

      // draw FFT level meter
      fft.exec(raw_data);
      size_t bw = gfx->width() / 30;
      if (bw < 3) { bw = 3; }
      int32_t dsp_height = gfx->height();
      int32_t fft_height = dsp_height - header_height - 1;
      size_t xe = gfx->width() / bw;
      if (xe > (FFT_SIZE/2)) { xe = (FFT_SIZE/2); }
      int32_t wave_next = ((header_height + dsp_height) >> 1) + (((256 - (raw_data[0] + raw_data[1])) * fft_height) >> 17);

      uint32_t bar_color[2] = { 0x000033u, 0x99AAFFu };

      for (size_t bx = 0; bx <= xe; ++bx)
      {
        size_t x = bx * bw;
        if ((x & 7) == 0) { gfx->display(); taskYIELD(); }
        int32_t f = fft.get(bx);
        int32_t y = (f * fft_height) >> 18;
        if (y > fft_height) { y = fft_height; }
        y = dsp_height - y;
        int32_t py = prev_y[bx];
        if (y != py)
        {
          gfx->fillRect(x, y, bw - 1, py - y, bar_color[(y < py)]);
          prev_y[bx] = y;
        }
        py = peak_y[bx] + 1;
        if (py < y)
        {
          gfx->writeFastHLine(x, py - 1, bw - 1, bgcolor(gfx, py - 1));
        }
        else
        {
          py = y - 1;
        }
        if (peak_y[bx] != py)
        {
          peak_y[bx] = py;
          gfx->writeFastHLine(x, py, bw - 1, TFT_WHITE);
        }

        if (wave_enabled)
        {
          for (size_t bi = 0; bi < bw; ++bi)
          {
            size_t i = x + bi;
            if (i >= gfx->width() || i >= WAVE_SIZE) { break; }
            y = wave_y[i];
            int32_t h = wave_h[i];
            bool use_bg = (bi+1 == bw);
            if (h>0)
            { /// erase previous wave.
              gfx->setAddrWindow(i, y, 1, h);
              h += y;
              do
              {
                uint32_t bg = (use_bg || y < peak_y[bx]) ? bgcolor(gfx, y)
                            : (y == peak_y[bx]) ? 0xFFFFFFu
                            : bar_color[(y >= prev_y[bx])];
                gfx->writeColor(bg, 1);
              } while (++y < h);
            }
            size_t i2 = i << 1;
            int32_t y1 = wave_next;
            wave_next = ((header_height + dsp_height) >> 1) + (((256 - (raw_data[i2] + raw_data[i2 + 1])) * fft_height) >> 17);
            int32_t y2 = wave_next;
            if (y1 > y2)
            {
              int32_t tmp = y1;
              y1 = y2;
              y2 = tmp;
            }
            y = y1;
            h = y2 + 1 - y;
            wave_y[i] = y;
            wave_h[i] = h;
            if (h>0)
            { /// draw new wave.
              gfx->setAddrWindow(i, y, 1, h);
              h += y;
              do
              {
                uint32_t bg = (y < prev_y[bx]) ? 0xFFCC33u : 0xFFFFFFu;
                gfx->writeColor(bg, 1);
              } while (++y < h);
            }
          }
        }
      }
      gfx->display();
      gfx->endWrite();
    }
  }

  if (!gfx->displayBusy())
  { // draw volume bar
    static int px;
    uint8_t v = M5Cardputer.Speaker.getVolume();
    int x = v * (gfx->width()) >> 8;
    if (px != x)
    {
      gfx->fillRect(x, 6, px - x, 2, px < x ? 0xAAFFAAu : 0u);
      gfx->display();
      px = x;
    }
  }
}
 

void setup(void)
{
  auto cfg = M5.config();
  cfg.external_speaker.hat_spk = true;
  M5Cardputer.begin(cfg);

  

  preallocateBuffer = malloc(preallocateBufferSize);
  preallocateCodec = malloc(preallocateCodecSize);

  { /// custom setting
    auto spk_cfg = M5Cardputer.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 128000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    M5Cardputer.Speaker.config(spk_cfg);
  }

  M5Cardputer.Speaker.begin();
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextColor(GREEN, BLACK);
  //M5Cardputer.Display.setTextFont(&fonts::FreeMono12pt7b);
  M5Cardputer.Display.setTextFont(&fonts::FreeMonoOblique9pt7b);    
   
  

  connectToWiFi();
  //sd_init(); 

  M5Cardputer.Display.clear() ; 
  
  getStationList();

  M5Cardputer.Display.print("Found ");
  M5Cardputer.Display.print(stationList.size());
  M5Cardputer.Display.println(" stations ");
  delay(1000); 

  M5Cardputer.Lcd.setTextSize(1);
  M5Cardputer.Display.clear();
  gfxSetup(&M5Cardputer.Display);

  preferences.begin("M5_settings", false);
  previousVolume = preferences.getInt("PreviousVolume", 50);
  station_index = preferences.getInt("PreviousStation", 1);
  preferences.end();
  
  play(station_index);
    //xTaskCreatePinnedToCore(decodeTask, "decodeTask", 4096, nullptr, 1, nullptr, PRO_CPU_NUM);
  xTaskCreatePinnedToCore(decodeTask, "decodeTask", 8096, nullptr, 1, nullptr, PRO_CPU_NUM);
}

void loop(void)
{
  wave_enabled = false;
  //fft_enabled  = false;
  gfxLoop(&M5Cardputer.Display);

  {
    static int prev_frame;
    int frame;
    do
    {
      delay(1);
    } while (prev_frame == (frame = millis() >> 3)); /// 8 msec cycle wait
    prev_frame = frame;
  }

  M5Cardputer.update();
  if (true){
      size_t v = M5Cardputer.Speaker.getVolume();
        if (M5Cardputer.Keyboard.isChange()) {
          M5Cardputer.Speaker.tone(550, 50);
          if (M5Cardputer.Keyboard.isKeyPressed('/')) {
            delay(200);
            M5Cardputer.Speaker.tone(1000, 100);
            if (++station_index >= stationList.size()) { station_index = 0; }
              play(station_index);
            }
            if (M5Cardputer.Keyboard.isKeyPressed(',')) {
            delay(200);
            M5Cardputer.Speaker.tone(800, 100);     
            //if (--station_index >= stations) { station_index = 0; }

            if (station_index <= 0) { station_index = stationList.size() - 1; } else { station_index--; }
            play(station_index);
            }
            if (M5Cardputer.Keyboard.isKeyPressed(';')) {
            if (v <= 255){
              v+= 10;
              M5Cardputer.Speaker.setVolume(v);
              previousVolume = M5Cardputer.Speaker.getVolume();
              }
            }
            if (M5Cardputer.Keyboard.isKeyPressed('.')) {
            if (v >= 0){
              v-= 10;
              M5Cardputer.Speaker.setVolume(v);
              previousVolume = M5Cardputer.Speaker.getVolume();
              }
            }
            if (M5Cardputer.Keyboard.isKeyPressed('m')) {
            if (M5Cardputer.Speaker.getVolume() > 0) {
              previousVolume = M5Cardputer.Speaker.getVolume();
              M5Cardputer.Speaker.setVolume(0);
            } else {
              M5Cardputer.Speaker.setVolume(previousVolume);
            }
          }
            preferences.begin("M5_settings", false);
            preferences.putInt("PreviousVolume", previousVolume);
            preferences.putInt("PreviousStation", station_index);
            preferences.end();
        }
    
    if (M5Cardputer.BtnA.wasPressed())
    {
      M5Cardputer.Speaker.tone(440, 50);
    }
    if (M5Cardputer.BtnA.wasDecideClickCount())
    {
      switch (M5Cardputer.BtnA.getClickCount())
      {
      case 1:
        M5Cardputer.Speaker.tone(1000, 100);
        if (++station_index >= stationList.size()) { station_index = 0; }
        play(station_index);
        break;

      case 2:
        M5Cardputer.Speaker.tone(800, 100);
        if (station_index == 0) { station_index = stationList.size(); }
        play(--station_index);
        break;
      }
    } 
  }
}