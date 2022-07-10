# arduino program for gamecube ver2
Arduino microを使ってGamecubeを操作するためのソースコード **（ver2）** です。

無印版（ver1）は[tangentialstar/
arduino_program_for_gamecube](https://github.com/tangentialstar/arduino_program_for_gamecube ) です。

## つくったきっかけ
ゲームキューブをArduinoで操作するためのプログラムが公開されており、感銘を受けて、私も早速試してみました。
ただ、ソースコード単品（人様が公開しているGUIに依存しない形）で、Arduinoでゲームキューブの操作を自動化してみたいと思ったので作ってみました。

## このプログラムでできること
シリアルモニタに`a`や`b`、`x`、`r`などを入力して、ゲームキューブを操作できます **（大文字と小文字は区別されます！）** 。
また、上下左右には`U`, `D`, `R`, `L`を入力してください。

詳しくは320行目`commandCheckAndConvert(void)`の中身を読んで下さい。


なお、予めプログラムに入力した「自動化コマンド（87行目）」を**実行するには、シリアルモニタで`p`を送信**してください。
また、コマンドを**確認するには、シリアルモニタで`q`を送信**してください。

プログラム中に書き込むことができるコマンド数は、ArduinoのRAMサイズに依存します。
Arduino microでは、**少なくとも「1KB分（＝512コマンド分）」の自動化列の記憶・操作が確認できました**。


![COM5 2022-07-10 15-36-17-0001](https://user-images.githubusercontent.com/107760099/178134183-fde2c53c-eacd-4460-9d48-dea06aff3d41.gif)


## ゲームキューブとの配線

Arduino microとゲームキューブの配線は、下記2本です。
 * デジタル5番ピン－ゲームキューブコントローラーの「データ線」
 * GND－ゲームキューブコントローラーの「GND線」

ただし、上記配線の中に、双方向レベル変換モジュールを噛ませる必要があるようです。
Arduino micro（5V）-ゲームキューブ（3.3V）で変換する必要があるので、それに注意して配線しましょう。

![IMG_2956](https://user-images.githubusercontent.com/107760099/178104884-67e520d4-35b6-411b-a827-51acd4d0fddc.jpg)

なお、ゲームキューブコントローラーから「データ線」「GND線」を取り出すためには、物理的な結線が必要です。
 * ゲームキューブに「GC to RJ-45 変換ケーブル」を接続し、それを「LAN DIPキット」に接続してピンとして取り出す
 * ゲームキューブに「GC用延長ケーブル」を接続し、そのケーブルの真ん中を切断して被覆の中からGND線とDATA線を取り出す


## 外部依存ライブラリ
外部依存ライブラリは、[NicoHood/Nintendo](https://pages.github.com/](https://github.com/NicoHood/Nintendo ) になります。


## つかいかた（How to edit source code to automate GC input）
**ソースコードの87行目**にある配列`autocommand[]`に、ご自身でコマンドを入力することで、自動化することができます。

入力する際には、必ず配列`autocommand[]`の最初には`COMMAND(KEY,BEGIN,0), `を、最後には`COMMAND(KEY,END,0), `を**必ず**挿入するようにしてください。
コマンドは、`COMMAND( KEYまたはSTICK  , ボタンまたは角度 , SEC(整数)  ), `で指定できます。

### キー入力(ボタン入力)をコマンドにしたい場合
コマンドは、`COMMAND( KEY , ボタン名 , SEC(長押し秒) ), `で指定できます。

この`ボタン名`に指定できるのは、`A`, `B`, `X`, `Y`, `R`, `L`, `START`, `RIGHT`, `LEFT`, `UP`, `DOWN`, `Z`です。
待機させたい場合は、コマンドに`WAIT`を指定します。
また、`長押し秒`は、**0秒から32.6秒までの範囲**でかつ0.1秒の単位まで、例えば`SEC(24.1)`のように指定します。

**※コマンド送信ループの中に時間待機を組み込んでいる仕様上、待機時間は「アバウト」です。正確な時間を要するプログラミングにはこのソースコードは不適です。**

### スティック入力をコマンドにしたい場合
コマンドは、`COMMAND( STICK , DEG(角度) , SEC(長押し秒) ), `で指定できます。

アナログスティックの角度は、**下方向を0度として、反時計回りに0～360の範囲で指定**できます。
なお、有効な角度の**分解能は「5度」**です（例えば、133度（`DEG(133)`）で指定しても**5度単位に切り捨てられ、130度となります**）。
また、`DEG()`マクロの中には、整数値を入力してください。

例として、下記に「0.2秒間スティックを倒して、0.5秒待機するプログラム」の書き方を示します。
配列「`autocommand[]`」に入れます。

入力する際には、必ず配列`autocommand[]`の最初には`COMMAND(KEY,BEGIN,0), `を、最後には`COMMAND(KEY,END,0), `を**必ず**挿入するようにしてください。

```
signed short int autocommand[]={
  // *** Please check carefully if the program will begin with `COMMAND(KEY,BEGIN,0),` ***
  COMMAND(KEY,BEGIN,0), 

  COMMAND(KEY  ,X       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // hold X button in 0.2sec and then wait 0.8sec
  COMMAND(KEY  ,B       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // hold B button in 0.2sec and then wait 0.8sec
  COMMAND(STICK,DEG(0)  , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 0 deg(=down direction). and then wait 0.5sec
  COMMAND(STICK,DEG(30) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 30 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(45) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 45 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(60) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 60 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(90) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 90 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(120), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 120 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(135), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 135 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(150), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 150 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(180), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 180 deg. and then wait 0.5sec
  COMMAND(KEY  ,X       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // hold X button in 0.2sec and then wait 0.8sec
  COMMAND(KEY  ,B       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // hold B button in 0.2sec and then wait 0.8sec
  COMMAND(STICK,DEG(210), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 210 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(225), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 225 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(240), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 240 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(270), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 270 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(300), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 300 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(315), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 315 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(330), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 330 deg. and then wait 0.5sec
  COMMAND(STICK,DEG(360), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // tilt the stick about 0.5sec in direction of 360 deg. and then wait 0.5sec
  COMMAND(KEY  ,X       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // hold X button in 0.2sec and then wait 0.8sec
  COMMAND(KEY  ,B       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // hold B button in 0.2sec and then wait 0.8sec

  COMMAND(KEY,END,0), 
  // *** Please check carefully if the program will end with `COMMAND(KEY,END,0),` ***
};

```

## 参考情報
Serial通信について、特に参考にしたのは、[mizuyoukanao/WHALE](https://github.com/mizuyoukanao/WHALE ) です。

無印版（ver1）は[tangentialstar/
arduino_program_for_gamecube](https://github.com/tangentialstar/arduino_program_for_gamecube ) です。



### 無印版（ver1）との違い
無印版（ver1）は[コチラ](https://github.com/tangentialstar/arduino_program_for_gamecube ) 。

無印版との主な違いは「コマンドのデータ構造の見直し」です。
1コマンドあたりに消費するRAMの容量を2byteに減らし、RAMが限られるArduino microでの複数コマンドの自動化を可能にしました。

従来は、コマンド列を構造体
```
struct{
  char command[11];
  long int holdtime;
} autocommand[];
```
で管理していたのに対して、今回のプログラムでは
```
signed short int autocommand[];
```
と改めました。
これにより、RAM容量1KB換算(1024byte)で比較すると、従来（15byte/1command）はわずか「66コマンド」しか記憶できなかったのに対し、
**今回（2byte/1command）は「500コマンド」も記憶することができる**ようになりました。
Arduino microに操作プログラムをハードコーディングする上での課題を解決する形となっています。

ただし、**スティック入力の角度が「5度」単位に制限される点、長押し時間および待機時間が「0.01秒」以下の単位が無視される点**には、注意してください。
（1度単位でのスティック操作が必要な場合は[ver1](https://github.com/tangentialstar/arduino_program_for_gamecube )をお使いください）

### コマンドデータ仕様のイメージ
short intの5桁の「下2桁」「上3桁」「プラスマイナスの別」でデータを管理しています。

![gc_command_image](https://user-images.githubusercontent.com/107760099/178135650-a4202e02-8e9e-49c5-8728-bf3a6504425d.png)

