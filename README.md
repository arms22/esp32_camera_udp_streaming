# Low Latency Camera Streaming with UDP for ESP32-Camera

OV2640搭載 Unit Cam Wi-Fi Camera向けのUDPを使った低遅延ストリーミングソフトウェアです。カメラから取得したデータを逐次を送信するため、PSRAMを持たないESP32でも高解像度（SVGA/HD）の画像を高いフレームレート（30p/15p）で低遅延でストリーミング可能です。受信側はPython+OpenCVで書いてます。

## 使い方（送信：esp32_sender）

ビルドにはESP-IDF 4.2が必要です。

### WiFi設定

`Example Configuration`でSSIDとパスワードを設定してください。

```
cd esp32_sender
idf.py menuconfig
```

### ビルドとフラッシュ

ビルドしてフラッシュしてシリアル出力を確認する。

```
cd esp32_sender
idf.py -p PORT build flash monitor
```

## 使い方（受信：python_receiver）

### OpenCVのインストール

```
pip3 install --upgrade pip
pip3 install opencv-python
```

### 実行

```
python3.exe python_receiver\receiver.py <listen_ip> <esp32_ip>
```
