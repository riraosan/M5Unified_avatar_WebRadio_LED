# M5Unified_avatar_WebRadio_LED_and_ES9038Q2M
M5Unified_avatar_WebRadio_LED_and_ES9038Q2M

@riraosan

このフォルダにあるファイル一式は、以下の機能を実現しています。

- レベルメーターとスタックチャンの顔をデジタルTVのコンポジットへ出力する（@riraosanが作成したESP32_8BIT_CVBSライブラリを使用）
- スタックチャンとレベルメーターを同じ画面へ出力する（スタックチャンのインスタンスにM5Canvasのポインタを渡せるように改修）
- WebRadioからのMP3再生をI2S経由で外部DAC(ES9038Q2M)へ出力する(MP3@192KHz, 44100Hz, 32bit※16bitでないことに注意してください）

@riraosanは次の機器構成でこのフォルダのサンプルプログラムが動作することを確認しました。

- M5STAC ATOM Lite
- ATOMIC TF-CARDモジュール
- ES9038Q2M 中華DAC（メーカー不明）
- デジタルTV（コンポジット入力付）

ATOM Lite以外で動作確認をしていません。

patchフォルダに@riraosanが改修したSpeaker_Class.cppとSpeaker_Class.hppを格納しています。i2s_writeをi2s_write_expandに置き換えました。ご使用の際は、M5Unifiedライブラリの同名ファイルを置き換えてください。もちろんサンプリングビットが32bitのDACが必要です。

DACのLOCKが度々外れる場合は、I2Sのバッファカウントとバッファレングスの値を調整してください。setupAudio()内で@riraosanのDACで最適化した設定をおこなっています。

platformio.iniを同梱しました。使用したライブラリはこのファイルを参照してください。

このサンプルを公開していただきました@robo8080さん、@lovyan03さんに感謝いたします。ありがとうございました。

どこかのどなたかのなんらかに貢献できましたら幸いです。

以下、オリジナルの本文です。

---

# M5Unified_avatar_WebRadio_LED
M5Unified_avatar_WebRadio_LED

![画像1](images/image1.png)<br><br>


OLED/LCD表示、LEDレベルメーター付きWebRadioファームです。<br>

WebRadioは、ESP8266Audio Libraryのexample"WebRadio"をベースにさせていただきました。<br>
FFTの表示は、M5Unifiedライブラリのexample"をベースにさせていただきました。<br>
Avatar表示は、meganetaaanさんのm5stack-avatarをベースにさせていただきました。<br>
オリジナルはこちら。<br>
An M5Stack library for rendering avatar faces <https://github.com/meganetaaan/m5stack-avator><br>

---
### 注意：未対策の不具合が有ります ###
~~まだ動作が不安定です。時々画面が真っ黒になったり、リブートしたりします。~~<br><br><br>


### このプログラムを動かすのに必要な物 ###
* [M5Stack](http://www.m5stack.com/ "Title") (M5Stack FireとM5Stack Core2 for AWSで動作確認をしました。)<br>
* [OLEDディスプレイユニット](https://www.switch-science.com/catalog/7233/ "Title") または [ LCDディスプレイユニット](https://www.switch-science.com/catalog/7358/ "Title")
* Arduino IDE (バージョン 1.8.15で動作確認をしました。)<br>
* [M5Unified](https://github.com/m5stack/M5Unified/tree/develop/ "Title")ライブラリ((developバージョンで動作確認をしました。))<br>
* [M5GFX](https://github.com/m5stack/M5GFX/tree/develop/ "Title")ライブラリ(バージョン 0.0.18で動作確認をしました。)<br>
* [ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio/ "Title")<br>
* [FastLED](https://github.com/FastLED/FastLED/ "Title")ライブラリ<br>
<br><br>

### 注意 ###

* M5Stackボードマネージャーのバージョン2.0.0で動作確認をしました。バージョン2.0.2ではビルドエラーになります。

---
### WiFiの設定 ###
* "M5Unified_avatar_WebRadio_LED.ino"の95行目付近、SSIDとPASSWORDを設定してください。

### OLED/LCDのAvatar表示を使用しない場合 ###

* "M5Unified_avatar_WebRadio_LED.ino"の43行目をコメントにしてください。

### M5Stack-SD-Updaterに対応させる場合 ###

* [M5Stack-SD-Updater](https://github.com/tobozo/M5Stack-SD-Updater/ "Title")ライブラリが必要です。<br>
* "M5Unified_avatar_WebRadio_LED.ino"の24行目のコメントを外して"#define USE_SD_UPDATER"を有効にします。<br>

M5Stack-SD-Updaterの使い方はこちらを参照してください。：<https://github.com/tobozo/M5Stack-SD-Updater>


---

### ボタン操作方法 ###
* ボタンA：組み込み済みのラジオ局を切り替えます。<br>
* ボタンB：音量を小さくします。<br>
* ボタンC：音量を大きくします。<br>

### WebUI操作方法 ###
1. M5M5Stackの電源を入れるとWiFi APに接続し、APから割り当てられたIPアドレスが表示されます。
2. 表示されたIPアドレスにブラウザでアクセスすると操作パネルが表示されます。
![画像4](images/image2.png)<br><br>
<br><br>


---


### WebRadioの参考URL ###
<http://wbgo.streamguys.net/thejazzstream><br>
<http://beatles.purestream.net/beatles.mp3><br>
<http://listen.181fm.com/181-beatles_128k.mp3><br>
<http://ice1.somafm.com/illstreet-128-mp3><br>
<http://ice1.somafm.com/secretagent-128-mp3><br>
<http://ice1.somafm.com/seventies-128-mp3><br>
<http://ice1.somafm.com/bootliquor-128-mp3><br>
<http://stream.srg-ssr.ch/m/rsj/mp3_128><br>
<http://icecast.omroep.nl/3fm-sb-mp3><br><br>



