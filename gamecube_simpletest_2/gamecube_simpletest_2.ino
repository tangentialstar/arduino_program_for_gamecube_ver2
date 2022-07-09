#include "Nintendo.h"

#define SERIAL_SPEED (4800) 
#define MS(x) ( (long int)((double)x/8.129608) )

#define NO_WAIT_PRINT     // デバッグ用のインフォメーションから「Wait」記述を消すための宣言
#define USE_POKEMON_CO_XD // コロシアム・XDのとき、スティック入力が正しい方向にならないバグを吸収するための宣言

 
// デジタル5ピンをコントローラーにつなぐ
CGamecubeConsole GamecubeConsole1(5); 

// 構造体「Gamecube_Data_t」が送信するデータ列のイメージ
Gamecube_Data_t ContollerData = defaultGamecubeData; 
// ※「defaultGamecubeData」はゲームキューブコントローラーの初期位置状態（？）
// 要するにニュートラルポジションだと思う。（GamecubeAPI.hの29行目に宣言されてた）

const int HOLDTIME = (30); // 1回のキー入力の長押し時間[ms]
const int PRESS   = 1;
const int RELEASE = 0;

char keyname[11]="";         // シリアル入力の1文字コマンドを文字列に変換する先（'D'→"DOWN", 'r'→"R", 'R'→"RIGHT"）
char current_command[11]=""; // 現在実行中の入力コマンド文字列を保存するもの
static volatile bool processing = false; // 実行中かどうかの判定フラグ


// 自動化コマンド実装用
bool automode = false;
int now=0;
struct{
  char command[11];
  long int holdtime;
} autocommand[]={
  // 自動化は{"コマンド", 長押しフレーム数}, で宣言するよ！
  // コマンドは、"A","B","X","Y","R","L","START","RIGHT","LEFT","UP","DOWN",
  // もしくは、スティック角度（"0"～"360"）で指定できるよ！
  // 長押しフレーム数は、だいたい1フレームあたり8.13ミリ秒に相当するよ！MS()のマクロに突っ込めば
  // 勝手に8.13で除してintにしてくれるよ！待機時間（フレーム）はlongなので整数で記入してね！
  // 待機したい時は"WAIT"で指定してね！
  // ★★★ここにコマンドを入力しよう★★★
  {"-", 0}, // 開始時は{"-",0}, で始めてね
  {"0",    MS(200)},{"WAIT", MS(500)},
  {"30",   MS(200)},{"WAIT", MS(500)},
  {"45",   MS(200)},{"WAIT", MS(500)},
  {"60",   MS(200)},{"WAIT", MS(500)},
  {"90",   MS(200)},{"WAIT", MS(500)},
  {"120",  MS(200)},{"WAIT", MS(500)},
  {"135",  MS(200)},{"WAIT", MS(500)},
  {"150",  MS(200)},{"WAIT", MS(500)},
  {"180",  MS(200)},{"WAIT", MS(500)},
  {"210",  MS(200)},{"WAIT", MS(500)},
  {"225",  MS(200)},{"WAIT", MS(500)},
  {"240",  MS(200)},{"WAIT", MS(500)},
  {"270",  MS(200)},{"WAIT", MS(500)},
  {"300",  MS(200)},{"WAIT", MS(500)},
  {"330",  MS(200)},{"WAIT", MS(500)},
  {"345",  MS(200)},{"WAIT", MS(500)},
  {"360",  MS(200)},{"WAIT", MS(500)},
  {"-", 0}, // 終了時も{"-",0}, で終わってね。以降は無視されます

  // 以下、メモ
  {"A",    MS(300)},{"WAIT", MS(1200)},
  {"A",    MS(300)},{"WAIT", MS(1200)},
  {"RIGHT",MS(70)},{"WAIT", MS(800)},
  {"A",    MS(300)},{"WAIT", MS(1200)},
  {"B",    MS(300)},{"WAIT", MS(400)},
  {"B",    MS(300)},{"WAIT", MS(400)},
  {"B",    MS(300)},{"WAIT", MS(400)},

  {"A",   120}, // 120と書いて実際には992ms→ 1[f]あたり約8.1[ms]
  {"B",   150}, // 1220ms
  {"B",   100}, // 799ms
  {"180", 280}, // 2284ms
  {"135", 160}, // 1310ms
  {"X",   150}, // 1217ms
  {"A",   180}, // 1459ms
  {"A",   180}, // 1455ms

};



void setReportNeutral(void); // コントローラー初期化用関数。
bool sendReport(void);       // コマンドをGCに送信するwrap関数。
void commandCheckAndConvert(void);     // PCからのシリアル通信で得たキー入力を文字列keyname[]に変換するだけの関数。
int Button(char* keyname, int PRESS_OR_RELEASE = 0);  // 第1引数に対応するキーのreport状態を書き換えるだけの関数。
inline bool checkprocessing(unsigned long timestamp); // timestampがHOLDTIMEを超えているかを返すだけ。
inline bool checkprocessing(unsigned long timestamp, unsigned long holdtime); // 同上だが第2引数でHOLDTIMEを指定可。

void setup(){

  // reportを初期化。
  setReportNeutral();

  //コントローラーを認識させるおまじない
  ContollerData.report.start = 1;
  GamecubeConsole1.write(ContollerData);  //press start
  ContollerData.report.start = 0;
  GamecubeConsole1.write(ContollerData);  //release start

  // PCとのシリアル通信
  Serial.begin(SERIAL_SPEED);

}

void loop() {
  static unsigned long timestamp = 0;

  if(automode){
    // ★自動コマンド実行モードが有効なら
    // i番目のコマンド(char*)：autocommand[i].command／待機時間(int)：autocommand[i].holdtime
    // iは変数nowで処理


    // ★ボタン押下処理autocommand[]を逐次実行していく。
    if( processing ){
      // なにもしない
    }else{
      now++; // 次のコマンドへ

      // ★コマンド終了判定
      if(strcmp(autocommand[now].command, "-")==0){ // "-"が来たら終了。
        automode=false;
        now = 0;
        Serial.println("****** AUTO MODE PROCESS END *****");
        return;
      }
      
      strcpy(current_command, autocommand[now].command); // current_commandに格納
      processing = true;                // ステータスを処理中に変更
      Button(current_command, PRESS);   // ボタン内容をcurrent_commandに更新
      timestamp = millis();             // タイムスタンプを現在時刻に更新
    }

    // ★ボタン押下判定（時間経過チェック）
    processing = checkprocessing(timestamp,  autocommand[now].holdtime);
    
    // ★ボタン押し込み終了処理 ココから
    if( processing==false ){
      // 処理中でなければ
      Button(current_command, RELEASE); // ボタン内容からそのキーを離す
      processing = false;               // ステータスを処理中に変更
      #ifdef NO_WAIT_PRINT
      if(strcmp(current_command,"WAIT")!=0){
      #endif
      Serial.print("process succeeded->");
      Serial.println(current_command);
      #ifdef NO_WAIT_PRINT
      }
      #endif
      strcpy(current_command, "");      // current_commandをクリア
    }
    
  }else{
    // コマンド入力（シリアル入力）モード
    
    // ★Serialからキーを入力（keyname[]に格納）
    commandCheckAndConvert();
  
    // ★ボタン押下処理 ココから（受け取ったコマンド1文字毎に判定するので重いかも）
    if(keyname[0]=='\0'||keyname[0]=='\n'){
      // 入力されたシリアルが空っぽ（""）なら何もしない
    }else{
      // なにかシリアルに入力されてて、
      
      if( processing ){
        // 現在処理中なら、割り込まない（なにもしない）
        Serial.print("*** Key '");Serial.print(keyname);Serial.println("' will be ignored! ***");
        strcpy(keyname, ""); // 本当は、スタックに入れるとかで実装すれば無視しなくて済むけど、実装予定はない！
      }else{
        // 処理中でなければ、コマンドとして受け付け
        strcpy(current_command, keyname); // keynameからcurrent_commandに格納
        strcpy(keyname, "");              // コマンド列を空っぽに修正
        processing = true;                // ステータスを処理中に変更
        Button(current_command, PRESS);   // ボタン内容をcurrent_commandに更新
        timestamp = millis();             // タイムスタンプを現在時刻に更新
      }
    }
    // ★ボタン押下処理 ココまで

    // ★ボタン押下判定（時間経過チェック。ここも毎回重たい。）
    processing = checkprocessing(timestamp);
  
    // ★ボタン押し込み終了処理 ココから
    if( processing==false ){
      // 処理中でなければ
      
      if( current_command[0] != '\0' && current_command[0] != '\n'){
        Button(current_command, RELEASE); // ボタン内容からそのキーを離す
        processing = false;               // ステータスを処理中に変更
        Serial.print("process succeeded->");
        Serial.println(current_command);
        strcpy(current_command, "");      // current_commandをクリア
      }
    }

  }




  // ループ中は常にsendReport()する。
  if( !sendReport() ){
    Serial.println("GC is not connected or detected!");
  }

  
}


inline bool checkprocessing(unsigned long timestamp){
  // コマンド実行中であれば
  if(millis() - timestamp <= HOLDTIME){
    return true;
  }else{
    return false;
  }
}
inline bool checkprocessing(unsigned long timestamp, unsigned long holdtime){
  // コマンド実行中であれば
  if(millis() - timestamp <= holdtime){
    return true;
  }else{
    return false;
  }
}




void commandCheckAndConvert(void){
  // シリアル通信のコマンドをkeyname[]に変換するだけ。
  // 要は、rをRキー、Rを「Right」（十字右）に区別するために文字列にしている。
  // もっと良い実装がある気がするけど、いいんだよ！！
  if( Serial.available()){
    uint8_t receive_key = Serial.read();
    if(receive_key == '\n')return; // 改行コードは無視（処理しない）
    switch(receive_key){
      case 'a':
        strcpy(keyname, "A");
        break;
      case 'b':
        strcpy(keyname, "B");
        break;
      case 'x':
        strcpy(keyname, "X");
        break;
      case 'y':
        strcpy(keyname, "Y");
        break;
      case 's':
        strcpy(keyname, "S");
        break;
      case 'z':
        strcpy(keyname, "Z");
        break;
      case 'r':
        strcpy(keyname, "R");
        break;
      case 'l':
        strcpy(keyname, "L");
        break;
      case 'U':
        strcpy(keyname, "UP");
        break;
      case 'D':
        strcpy(keyname, "DOWN");
        break;
      case 'R':
        strcpy(keyname, "RIGHT");
        break;
      case 'L':
        strcpy(keyname, "LEFT");
        break;
      case 'w': case 'W':
        strcpy(keyname, "WAIT");
        break;

      // コマンド自動入力モードON！
      case 'p': case 'P':
        automode=true;
        now = 0;
        Serial.println("\n****** AUTO MODE PROCESS START *****");
        break;
      default:
        strcpy(keyname, "");
      break;
    }
    // 重くなるのでけした
    // Serial.println("\t--------in commandCheckAndConvert()--------");
    // Serial.print("\tkeyname->"); Serial.println(keyname);
    // Serial.println("\t--------in commandCheckAndConvert()--------");
  }
  return;
}


// キー入力を反映させるやつ。keynameに対応するキーをreportに1/0でセットするだけ。
int Button(char* keyname, int PRESS_OR_RELEASE = 0){
  // Serial.print("in function Button: ");
  int keylen = (int)strlen(keyname);
  int deg = atoi(keyname);
  double rad = 0;
  int x, y;

  // ★スティック判定（0～360を想定）★★★ここがなんかおかしい～～～～～！！！！（2022/07/04 23:54）
  if( '0' <= keyname[0] && keyname[0] <= '9'){
    // 入力されたkeyname[]が数字だった場合：
    if(PRESS_OR_RELEASE == PRESS){
      // PRESSでスティックを倒す
      Serial.println("Stick.Down");
      rad = (double)deg*PI/180.0; // 弧度法(ラジアン)変換
     // char buf[23];
      // sprintf(buf, "\tdeg=%3d,rad=",deg);     
      // Serial.print(buf); Serial.println(rad);
      x = (double)128*sin(rad);
      y = (double)-128*cos(rad);
      x += 128; y += 128;
      if(x >= 255) x=255; if(x <= 0) x=0;
      if(y >= 255) y=255; if(y <= 0) y=0;
      #ifdef USE_POKEMON_CO_XD 
      if(y<=0)y=1; // ポケモンCo,XDだと0を与えると255になるバグがあるので1にする
      #endif
      ContollerData.report.xAxis = x;
      ContollerData.report.yAxis = y;
      // sprintf(buf, "\t(x=%3d,y=%3d)",x, y);
      // Serial.println(buf);       
    }else{
      // RELEASEだったらスティックを初期位置（＝128）に戻す
      Serial.println("Stick.UP");
      ContollerData.report.xAxis = 128;
      ContollerData.report.yAxis = 128;
    }
    return deg;
  }

  // ★キー入力判定
  if( keylen == 1){
    switch(keyname[0]){
      case 'a': case 'A':
        ContollerData.report.a = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.A"); else Serial.println("KeyUp.A");
        break;
      case 'b': case 'B':
        ContollerData.report.b = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.B"); else Serial.println("KeyUp.B");
        break;
      case 'x': case 'X':
        ContollerData.report.x = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.X"); else Serial.println("KeyUp.X");
        break;
      case 'y': case 'Y':
        ContollerData.report.y = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Y"); else Serial.println("KeyUp.Y");
        break;
      case 'l': case 'L':
        ContollerData.report.l = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.L"); else Serial.println("KeyUp.L");
        break;
      case 'r': case 'R':
        ContollerData.report.r = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.R"); else Serial.println("KeyUp.R");
        break;
      case 'z': case 'Z':
        ContollerData.report.z = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Z"); else Serial.println("KeyUp.Z");
        break;
      case 's': case 'S':
        ContollerData.report.start = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.S"); else Serial.println("KeyUp.S");
        break;
    }
  }else if( keylen >= 2){
    switch(keyname[0]){
      case 'r': case 'R': // right
        ContollerData.report.dright = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Right"); else Serial.println("KeyUp.Right");
      break;
      case 'l': case 'L': // left
        ContollerData.report.dleft = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Left"); else Serial.println("KeyUp.Left");
      break;
      case 'u': case 'U': // up
        ContollerData.report.dup = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Up"); else Serial.println("KeyUp.Up");
      break;
      case 'd': case 'D': // down
        ContollerData.report.ddown = PRESS_OR_RELEASE;
        if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Down"); else Serial.println("KeyUp.Down");
      break;
      case 'w': case 'W': // wait
        #ifndef NO_WAIT_PRINT
        if(PRESS_OR_RELEASE == PRESS)Serial.println("Waiting..."); else Serial.println("Wait end...");
        #endif
      break;
    }
    
  }else{
    return -1;
  }
  return keylen;
}

void setReportNeutral(void){
  // ContollerData = defaultGamecubeData;
  // レポート内容を初期化
  ContollerData.report.a = 0;
  ContollerData.report.b = 0;
  ContollerData.report.x = 0;
  ContollerData.report.y = 0;
  ContollerData.report.start = 0;
  ContollerData.report.dleft = 0;
  ContollerData.report.dright = 0;
  ContollerData.report.ddown = 0;
  ContollerData.report.dup = 0;
  ContollerData.report.z = 0;
  ContollerData.report.r = 0;
  ContollerData.report.l = 0;
  ContollerData.report.xAxis = 128;
  ContollerData.report.yAxis = 128;
  ContollerData.report.cxAxis = 128;
  ContollerData.report.cyAxis = 128;
  ContollerData.report.left = 0;
  ContollerData.report.right = 0;  
  return;
}

bool sendReport(void){
  return GamecubeConsole1.write(ContollerData);
  // write関数のラッパー関数。
  // write()は3種類あり、引数の型に応じて呼び出し先が変わる。
  // 今回は「GamecubeAPI.hpp」181行目のGamecube_Data_t型を要求する関数っぽい。
  // bool CGamecubeConsole::write(Gamecube_Data_t &data)
  // 内部でuint8_t gc_write()が呼ばれており、gc_n64_get→gc_n64_sendで実際のコマンド送信かな？
  // その返り値retがdata.status.rumbleを必要に応じて書き換える。→GCコンが震えているかはtrue/falseで判別可能
  // いずれにしても、何かしらの送信が成功するとretが3,4,5のいずれかになるっぽい。→trueが帰ってくる
}
