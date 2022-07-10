#include "Nintendo.h"

#define SERIAL_SPEED (4800) 

// ★ミリ秒→フレームに変換する
#define MS(VALI)       ( (float)((float)VALI/8.129608) )

// ★ボタン/スティックの係数
#define KEY         (-1)
#define STICK       (1)
// スティックの角度を5度刻みで変換（分解能5度）
#define DEG(deg) ((short int)deg/5)
// 与えられた秒（例：15.46）を整数部2桁・小数第1位に改める（例：15.4）ための変換。32.6秒を上回る待機時間は32.6に上書きする
#define SEC(se)     ( se > 32.6 ? 32.6 : ( (float)( (short int)( (float)se *10.0) ) / 10.0) )
// コマンド（short int）に変換するマクロ
#define COMMAND(type,keyname,holdtime) \
                    ( (short int)type * ( (short int)( (float)holdtime * 1000.0) + (short int)keyname ) )
// 長押し0秒、コマンド無し(ASCII='c')
#define DUMMYCOMMAND (-99) // これと同値→ COMMAND(KEY,'c', SEC(0.0)) なはず。

// コマンドのキーマップ
#define BEGIN ((signed short int)'-')
#define A     ((signed short int)'A')
#define B     ((signed short int)'B')
#define X     ((signed short int)'X')
#define Y     ((signed short int)'Y')
#define R     ((signed short int)'R')
#define L     ((signed short int)'L')
#define S     ((signed short int)'S')
#define START ((signed short int)'S')
#define Z     ((signed short int)'Z')
#define RIGHT ((signed short int)'K')
#define LEFT  ((signed short int)'H')
#define UP    ((signed short int)'U')
#define DOWN  ((signed short int)'J')
#define W     ((signed short int)'W')
#define WAIT  ((signed short int)'W')
#define END   ((signed short int)'-')

// マクロ展開例１：Aボタンを1.5秒長押し（小数第2位以下0.03は[無視]）
// COMMAND( KEY, A, SEC(1.53) ) 
//   →展開→  COMMAND( (-1), 'A', ( 1.53 > 32.6 ? 32.6 : ( (float)( (short int)( (float)1.53 *10.0) ) / 10.0) ) )  
//   →展開→  ( (short int)(-1) * ( (short int)( (float)( 1.53 > 32.6 ? 32.6 : ( (float)( (short int)( (float)1.5 *10.0) ) / 10.0) ) * 1000.0) + (short int)'A' ) )
//   →式評価→( (short int)(-1) * ( (short int)( (float)( ( (float)( (short int)( (float)1.53 *10.0) ) / 10.0) ) * 1000.0) + (short int)65 ) )
//   →計算→  ( (short int)(-1) * ( (short int)( (float)( ( (float) ( (short int)(15.3) ) / 10.0) ) * 1000.0) + (short int)65 ) )
//   →計算→  ( (short int)(-1) * ( (short int)( (float)( ( (float) ( 15 ) / 10.0) ) * 1000.0) + (short int)65 ) )
//   →計算→  ( (short int)(-1) * ( (short int)( (float)( 1.5 * 1000.0) + (short int)65 ) )
//   →計算→  ( (short int)(-1) * ( (short int)( 1500 ) + (short int)65 ) )
//   →計算→  ( (short int)(-1) * (  1565 )
//   →計算→  ( -1565 )
// 
// マクロ展開例２：スティックを130度の角度に11.6秒倒し続ける（133は分解能に対応していないので5度刻みで[切り捨て]）
// COMMAND( STICK, DEG(133), SEC(11.6) ) 
//   →展開→   ( (short int)(1) * ( (short int)( (float)( 11.6 > 32.6 ? 32.6 : ( (float)( (short int)( (float)11.6 *10.0) ) / 10.0) ) * 1000.0) + (short int)((short int)133/5) ) )
//   →式評価→ ( (short int)(1) * ( (short int)( (float)(  ( (float)( (short int)( (float)11.6 *10.0) ) / 10.0) ) * 1000.0) + (short int)((short int)133/5) ) )
//   →計算→   ( (short int)(1) * ( (short int)( (float)(  (  11.6 * 1000.0) + (short int)((short int)133/5) ) )
//   →計算→   ( (short int)(1) * ( (short int)( (float)(  11600 + (short int)(26) ) )
//   →計算→   ( (short int)(1) * ( (short int)( (float)(  11626 ) )
//   →計算→   ( (short int)(1) * 11626 )
//   →計算→   ( 11626 )


#define NO_WAIT_PRINT     // デバッグ用のインフォメーションから「Wait」記述を消すための宣言
#define USE_POKEMON_CO_XD // コロシアム・XDのとき、スティック入力が正しい方向にならないバグを吸収するための宣言

 
// デジタル5ピンをコントローラーにつなぐ
CGamecubeConsole GamecubeConsole1(5); 

// 構造体「Gamecube_Data_t」が送信するデータ列のイメージ
Gamecube_Data_t ContollerData = defaultGamecubeData; 
// ※「defaultGamecubeData」はゲームキューブコントローラーの初期位置状態（？）
// 要するにニュートラルポジションだと思う。（GamecubeAPI.hの29行目に宣言されてた）

const int HOLDTIME = MS(300); // 1回のキー入力の長押し時間[ms]
const int PRESS   = 1;
const int RELEASE = 0;

signed short int keyname = 0;         // 入力コマンド（キーの名前。すなわち負数のASCII(-1*'A')や、角度を示す整数(138)など）
signed short int current_command = 0; // 現在実行中のコマンドを保存するためのもの
static volatile bool processing = false; // 実行中かどうかの判定フラグ


// 自動化コマンド実装用
bool automode = false;
int now=0;
signed short int autocommand[]={
  // コマンドは、マクロCOMMAND()を使って定義しましょう。
  // COMMAND()の中身は、STICKまたはKEY, DEG(133)または'A' , SEC(11.6)のように指定します。
  // 2個めで使えるのは大文字のみです（小文字は場合によっては無効またはスティック入力になります）。
  // ボタン：A, B, X, Y, R, L, S(またはSTART), Z
  // 十字キー：RIGHT, LEFT, UP, DOWN（マクロで指定してあります）
  // スティック：DEG(0)～DEG(360) の5度刻み。
  // ★★★ここにコマンドを入力しよう★★★

  // 開始時はCOMMAND(KEY,BEGIN,0),
  COMMAND(KEY,BEGIN,0), 
  COMMAND(KEY  ,X       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // Xボタン
  COMMAND(KEY  ,B       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // Bボタン
  COMMAND(STICK,DEG(0)  , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック0度
  COMMAND(STICK,DEG(30) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック30度
  COMMAND(STICK,DEG(45) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック45度
  COMMAND(STICK,DEG(60) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック60度
  COMMAND(STICK,DEG(90) , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック90度
  COMMAND(STICK,DEG(120), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック120度
  COMMAND(STICK,DEG(135), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック135度
  COMMAND(STICK,DEG(150), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック150度
  COMMAND(STICK,DEG(180), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック180度
  COMMAND(KEY  ,X       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // Xボタン
  COMMAND(KEY  ,B       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // Bボタン
  COMMAND(STICK,DEG(210), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック210度
  COMMAND(STICK,DEG(225), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック225度
  COMMAND(STICK,DEG(240), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック240度
  COMMAND(STICK,DEG(270), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック270度
  COMMAND(STICK,DEG(300), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック300度
  COMMAND(STICK,DEG(315), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック315度
  COMMAND(STICK,DEG(330), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック330度
  COMMAND(STICK,DEG(360), SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.5)), // スティック360度
  COMMAND(KEY  ,X       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // Xボタン
  COMMAND(KEY  ,B       , SEC(0.2)), COMMAND(KEY, WAIT, SEC(0.8)), // Bボタン
  COMMAND(KEY,END,0), 
  // 終了時はCOMMAND(KEY,END,0), 以降は無視されます。


};

// GC操作用
void setReportNeutral(void); // コントローラー初期化用関数。
bool sendReport(void);       // コマンドをGCに送信するwrap関数。

// 今回の自動化用の関数たち
void commandCheckAndConvert(void); // PCからのシリアル通信で得たキー入力を文字列keyname[]に変換するだけの関数。
int Button(signed short int commandname, int PRESS_OR_RELEASE = 0); // 第1引数に対応するキーのreport状態を書き換えるだけの関数。
inline bool checkprocessing(unsigned long timestamp); // timestampがHOLDTIMEを超えているかを返すだけ。
inline bool checkprocessing(unsigned long timestamp, short holdtime); // 同上だが第2引数でHOLDTIMEを指定可。

// short intのコマンドを分離するための関数。第2引数～第4引数に格納します
void convertCommand(const signed short int command,      // input
                    signed short int *stick_or_key,      // output
                    signed short int *keyname_or_degree, // output
                    signed short int *hold_time          // output
){
  // コマンドの正負により「ボタン(KEY)」か「スティック(STICK)」かを判定
  *stick_or_key = (command <= 0 ? KEY : STICK );

  // 上3桁を抽出。最大値32600[ms]
  *hold_time = ((signed short int) abs(command) )/100; 
  if(*hold_time > 326) *hold_time = 326;
  *hold_time *= 100; // msに変換

  // 下2桁を抽出。
  *keyname_or_degree = ((signed short int)abs(command)) % 100;
  if(*stick_or_key == STICK) *keyname_or_degree *= 5; // スティックの場合は5倍して逆変換

  return;
}


// short intのコマンドの正負により「ボタン(KEY)」か「スティック(STICK)」かを判定
signed short int returnStickOrKey(const signed short int command){
  return (command <= 0 ? KEY : STICK );
}


// short intのコマンドの上3桁を抽出し、ms単位に戻して返す
signed short int returnHoldtime(const signed short int command){
  // 上3桁を抽出。最大値32600[ms]
  signed short int returnvalue = ((signed short int) abs(command) )/100; 
  if(returnvalue > 326) returnvalue = 326;
  returnvalue *= 100; // msに変換
  return returnvalue;
}


// short intのコマンドの下2桁を抽出し、数字（最大3桁）か、文字コード（2桁）を返す
// 注意：返却される数値（例：45）だけでは「角度」か「文字コード」かが判別できません。
// 　　　たとえば、if(returnKeyname(command)==END) では、ENDが'-'すなわち45なので、角度と誤認する可能性があります！
// 　　　したがって、条件で使う際はreturnStickOrKey(command) == KEY　などを併記するようにしてください。
signed short int returnKeyname(const signed short int command){
  // 下2桁を抽出。
  signed short int returnvalue = ((signed short int)abs(command)) % 100;
  if(returnStickOrKey(command) == STICK) returnvalue *= 5; // スティックの場合は5倍して逆変換
  return returnvalue;
}



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

    // ★ボタン押下処理autocommand[]を逐次実行していく。
    if( processing ){
      // なにもしない
    }else{
      now++; // 次のコマンドへ

      // ★コマンド終了判定
      if(returnStickOrKey(autocommand[now]) == KEY && returnKeyname(autocommand[now]) == END){ // ENDが来たら終了。
        automode=false;
        Serial.print("****** AUTO MODE PROCESS (now=");
        Serial.print(now);
        Serial.println(")END *****");
        now = 0;
        return;
      }
      
      current_command = autocommand[now];
      processing = true;                // ステータスを処理中に変更
      Button(current_command, PRESS);   // ボタン内容をcurrent_commandに更新
      timestamp = millis();             // タイムスタンプを現在時刻に更新
    }

    // ★ボタン押下判定（時間経過チェック）→このときに[ミリ秒]から[フレーム]に変換する
    processing = checkprocessing(timestamp, MS(returnHoldtime(autocommand[now]) ) );
    
    // ★ボタン押し込み終了処理 ココから
    if( processing==false ){
      // 処理中でなければ
      Button(current_command, RELEASE); // ボタン内容からそのキーを離す
      processing = false;               // ステータスを処理中に変更
      Serial.print("process succeeded(");
      Serial.print(now);
      Serial.print(")->");
      debugPrintCommand(current_command);
      current_command= DUMMYCOMMAND;      // current_commandをクリア
    }
    
  }else{
    // コマンド入力（シリアル入力）モード
    
    // ★Serialからキーを入力（keyname[]に格納）
    commandCheckAndConvert();
  
    // ★ボタン押下処理 ココから（受け取ったコマンド1文字毎に判定するので重いかも）
    if(keyname==DUMMYCOMMAND){
      // 入力されたシリアルが空っぽなら何もしない
    }else{
      // なにかシリアルに入力されてて、
      
      if( processing ){
        // 現在処理中なら、割り込まない（なにもしない）
        Serial.print("*** Key '");Serial.print((char)returnKeyname(keyname));Serial.println("' will be ignored! ***");
        keyname = DUMMYCOMMAND; // 本当は、スタックに入れるとかで実装すれば無視しなくて済むけど、実装予定はない！
      }else{
        // 処理中でなければ、コマンドとして受け付け
        current_command= keyname; // keynameからcurrent_commandに格納
        keyname = DUMMYCOMMAND;   // コマンド列を空っぽに修正
        processing = true;                // ステータスを処理中に変更
        Button(current_command, PRESS);   // ボタン内容をcurrent_commandに更新
        timestamp = millis();             // タイムスタンプを現在時刻に更新
      }
    }
    // ★ボタン押下処理 ココまで

    // ★ボタン押下判定（時間経過チェック。）
    processing = checkprocessing(timestamp);
  
    // ★ボタン押し込み終了処理 ココから
    if( processing==false ){
      // 処理中でなければ
      
      if( current_command != DUMMYCOMMAND){
        Button(current_command, RELEASE); // ボタン内容からそのキーを離す
        processing = false;               // ステータスを処理中に変更
        Serial.print("process succeeded->");
        debugPrintCommand(current_command);
        current_command= DUMMYCOMMAND;      // current_commandをクリア
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
inline bool checkprocessing(unsigned long timestamp, short holdtime){
  // コマンド実行中であれば
  if(millis() - timestamp <= (unsigned long)holdtime){
    return true;
  }else{
    return false;
  }
}



void commandCheckAndConvert(void){
  // シリアル通信のコマンドをkeynameに変換するだけ。
  if( Serial.available()){
    uint8_t receive_key = Serial.read();
    if(receive_key == '\n')return; // 改行コードは無視（処理しない）
    switch(receive_key){
      case 'a':
        keyname = COMMAND(KEY ,A    , SEC(0.2));
        break;
      case 'b':
        keyname = COMMAND(KEY ,B    , SEC(0.2));
        break;
      case 'x':
        keyname = COMMAND(KEY ,X    , SEC(0.2));
        break;
      case 'y':
        keyname = COMMAND(KEY ,Y    , SEC(0.2));
        break;
      case 's':
        keyname = COMMAND(KEY ,START, SEC(0.2));
        break;
      case 'z':
        keyname = COMMAND(KEY ,Z    , SEC(0.2));
        break;
      case 'r':
        keyname = COMMAND(KEY ,R    , SEC(0.2));
        break;
      case 'l':
        keyname = COMMAND(KEY ,L    , SEC(0.2));
        break;
      case 'U':
        keyname = COMMAND(KEY ,UP   , SEC(0.2));
        break;
      case 'D':
        keyname = COMMAND(KEY ,DOWN , SEC(0.2));
        break;
      case 'R':
        keyname = COMMAND(KEY ,RIGHT, SEC(0.2));
        break;
      case 'L':
        keyname = COMMAND(KEY ,LEFT , SEC(0.2));
        break;
      case 'w': case 'W':
        keyname = COMMAND(KEY ,WAIT , SEC(0.2));
        break;

      // コマンド自動入力モードON！
      case 'p': case 'P':
        automode=true;
        now = 0;
        Serial.println("\n****** AUTO MODE PROCESS START *****");
        break;

      // デバッグ用（autocommand[]の中身をシリアルモニタに印字）
      case 'q':
        Serial.println("***Debug info***");
        for(int k=0; k < sizeof(autocommand)/sizeof(signed short int) ; k++ ){
          Serial.print("\t");
          Serial.print(k);
          Serial.print("番目：");
          debugPrintCommand(autocommand[k]);
        }
        break;      

      default:
        keyname = DUMMYCOMMAND;
      break;
    }
  }
  return;
}

void debugPrintCommand(signed short int command){
  if(command == DUMMYCOMMAND){
    Serial.print("「");
    Serial.print(command);
    Serial.print("」\t");
    Serial.println("ダミーデータ");
    return;
  }
  signed short int sk=1;
  signed short int kn='-';
  signed short int ht=100;
  convertCommand(command, &sk, &kn, &ht);
  Serial.print("「");
  Serial.print(command);
  Serial.print("」\t");
  if(sk == KEY){
    if(kn == WAIT){
      Serial.print(ht);
      Serial.println("ミリ秒「待機」");
    }else if(kn == BEGIN || kn == END){
      Serial.println("***COMMAND BEGIN or END***");
    }else{
      Serial.print("キー「");
      Serial.print((char)kn);
      Serial.print("」を");
      Serial.print(ht);
      Serial.println("ミリ秒、長押し");
    }
  }else{
    Serial.print("スティックを「");
    Serial.print(kn);
    Serial.print("」度で");
    Serial.print(ht);
    Serial.println("ミリ秒、長押し");
  }
  return;
}

// キー入力を反映させるやつ。commandnameからkeynameを抽出して、対応するキーをreportに1/0でセットするだけ。
int Button(signed short int commandname, int PRESS_OR_RELEASE = 0){

  signed short int deg = returnKeyname(commandname);
  float rad = 0;
  int x, y;

  if( returnStickOrKey(commandname) == STICK ){
    if(PRESS_OR_RELEASE == PRESS){
      // PRESSでスティックを倒す
      Serial.println("Stick.Down");
      rad = (float)deg*PI/180.0; // 弧度法(ラジアン)変換
      x = (float)128*sin(rad);
      y = (float)-128*cos(rad);
      x += 128; y += 128;
      if(x >= 255) x=255; if(x <= 0) x=0;
      if(y >= 255) y=255; if(y <= 0) y=0;
      #ifdef USE_POKEMON_CO_XD 
      if(y<=0)y=1; // ポケモンCo,XDだと0を与えると255になるバグがあるので1にする
      #endif
      ContollerData.report.xAxis = x;
      ContollerData.report.yAxis = y;
    }else{
      // RELEASEだったらスティックを初期位置（＝128）に戻す
      Serial.println("Stick.UP");
      ContollerData.report.xAxis = 128;
      ContollerData.report.yAxis = 128;
    }
    return deg;
  }

  // ★キー入力判定
  switch(deg){
    case A:
      ContollerData.report.a = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.A"); else Serial.println("KeyUp.A");
      break;
    case B:
      ContollerData.report.b = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.B"); else Serial.println("KeyUp.B");
      break;
    case X:
      ContollerData.report.x = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.X"); else Serial.println("KeyUp.X");
      break;
    case Y:
      ContollerData.report.y = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Y"); else Serial.println("KeyUp.Y");
      break;
    case L:
      ContollerData.report.l = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.L"); else Serial.println("KeyUp.L");
      break;
    case R:
      ContollerData.report.r = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.R"); else Serial.println("KeyUp.R");
      break;
    case Z:
      ContollerData.report.z = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Z"); else Serial.println("KeyUp.Z");
      break;
    case START:
      ContollerData.report.start = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.S"); else Serial.println("KeyUp.S");
      break;
    case RIGHT:
      ContollerData.report.dright = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Right"); else Serial.println("KeyUp.Right");
    break;
    case LEFT:
      ContollerData.report.dleft = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Left"); else Serial.println("KeyUp.Left");
    break;
    case UP:
      ContollerData.report.dup = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Up"); else Serial.println("KeyUp.Up");
    break;
    case DOWN:
      ContollerData.report.ddown = PRESS_OR_RELEASE;
      if(PRESS_OR_RELEASE == PRESS)Serial.println("KeyDown.Down"); else Serial.println("KeyUp.Down");
    break;
    case WAIT:
      if(PRESS_OR_RELEASE == PRESS)Serial.println("Waiting..."); else Serial.println("Wait end...");
    break;
    default:
    break;
  }
  return deg;
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
