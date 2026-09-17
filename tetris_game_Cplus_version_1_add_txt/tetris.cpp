#include <iostream> 
#include <string>
#include <array>
#include <random>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cmath>

// Keyboard工具类
#include <conio.h>
#include <windows.h>// 如果需要 Sleep

// 输入输出流文件
#include <fstream> 
#include <sstream> 
using namespace std;

// 定义宏变量
static int constexpr WIDTH = 10;
static int constexpr HIGHT = 20;

// 定义全局变量
uint8_t g_dice = -1, g_nextDice = 0; // 随机获取一个方块，同时记录下一个方块
uint8_t g_rotateNum = 0; // 设置旋转参数
uint8_t g_nowCeil, g_nowLeft; // 设置方块（4 × 4）的左上角坐标
uint32_t g_level = 1, g_temporaryLevel = 1; // 设置初始等级
uint32_t g_score = 0, g_lines = 0; // 记录分数、消去的行数
bool g_quit = false; // 全局退出标志
array<array<char, WIDTH + 5>, 2 * HIGHT + 5> buffer = {}, temporaryBuffer = {};
int temporaryBlock[4][4] = {
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0}
};

// 读入读出数据地址
const string path = "history.txt";

const bool THEEND[8][41] = {
  {true,  true,  true,  true,  true,  true, false,  true,  true, false, false,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true, false,  true,  true, false,  true,  true,  true, false, false, false},
  {true,  true,  true,  true,  true,  true, false,  true,  true, false, false,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true, false,  true,  true, false,  true,  true,  true,  true, false, false},
  {false, false, true,  true, false, false, false,  true,  true, false, false,  true,  true, false,  true,  true, false, false, false, false, false,  true,  true, false, false, false, false, false,  true,  true,  true, false,  true,  true, false,  true,  true, false,  true,  true, false},
  {false, false, true,  true, false, false, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true, false,  true,  true, false,  true,  true, false, false,  true,  true},
  {false, false, true,  true, false, false, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true, false,  true,  true,  true, false,  true,  true, false, false,  true,  true},
  {false, false, true,  true, false, false, false,  true,  true, false, false,  true,  true, false,  true,  true, false, false, false, false, false,  true,  true, false, false, false, false, false,  true,  true, false,  true,  true,  true, false,  true,  true, false,  true,  true, false},
  {false, false, true,  true, false, false, false,  true,  true, false, false,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true, false,  true,  true,  true, false,  true,  true,  true,  true, false, false},
  {false, false, true,  true, false, false, false,  true,  true, false, false,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true,  true,  true,  true,  true, false,  true,  true, false,  true,  true,  true, false,  true,  true,  true, false, false, false}
};
  
static const int baseScore[] = {0, 100, 300, 500, 800}; // 标准基础分

// 全局静态引擎（随机数，只初始化一次）
static std::mt19937& get_engine() {
  static std::mt19937 engine(std::random_device{}());
  return engine;
}

// 定义结构体
struct Color {
  uint8_t r, g, b;
};

// 定义写入数据类型（score(int32_t), level(int8_t), line(int8_t)）
struct Record {
  int32_t score;
  int8_t level;
  int8_t line;
};

// 定义类类型
class DropTimer {
public:
  DropTimer(int intervalMs) : m_interval(intervalMs) {
    m_lastTime = std::chrono::steady_clock::now();
  }

  // 检查是否到达下落时间，如果到达则重置计时器并返回true
  bool isTimeToDrop() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTime);
    if (elapsed.count() >= m_interval) {
      m_lastTime = now;
      return true;
    }
    return false;
  }

  // 更新下落间隔（等级变化时使用）
  void setInterval(int intervalMs) {
    m_interval = intervalMs;
  }

private:
  // 定义私有变量m_interval为int类型，初始化m_lastTime为当前时间点
  int m_interval;
  std::chrono::steady_clock::time_point m_lastTime;
};

// 定义枚举类型
// 设置颜色代码
enum TetrisColor {
  COLOR_EMPTY = 0, // 用0表示空格
  COLOR_CYAN, // 1: I
  COLOR_YELLOW, // 2: O
  COLOR_PURPLE, // 3: T
  COLOR_GREEN, // 4: S
  COLOR_RED, // 5: Z
  COLOR_BLUE, // 6: J
  COLOR_ORANGE // 7: L
};

// 按键动作枚举
enum KeyAction {
  KEY_NONE,
  KEY_UP,
  KEY_DOWN,
  KEY_SPACE,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_QUIT
};

// 定义固定元素构成
// 定义rgb三色代码
const Color COLORS_CHART[] = {
  {255, 255, 255},
  {0, 159, 218},
  {254, 203, 0},
  {149, 45, 152},
  {105, 190, 40},
  {237, 41, 57},
  {0, 101, 189},
  {255, 121, 0}
};

// 设置方块代码
const int SHAPES[7][4][4] = {
  // I
  { 
    {0, 0, 0, 0},
    {1, 1, 1, 1},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  // O
  {
    {2, 2, 0, 0}, 
    {2, 2, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  // T
  { 
    {0, 3, 0, 0},
    {3, 3, 3, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  // S
  {
    {0, 4, 4, 0},
    {4, 4, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  // Z
  { 
    {5, 5, 0, 0},
    {0, 5, 5, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  // J
  {
    {6, 0, 0, 0},
    {6, 6, 6, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  // L
  { 
    {0, 0, 7, 0},
    {7, 7, 7, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  }
};

// 设置翻转形状后方块的代码（按上顺时针旋转）
// I 1 两种形状
const int rotateI[2][4][4] = {
  {
    {0, 0, 0, 0},
    {1, 1, 1, 1},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  { 
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 0, 0}
  }
};

// O 2 一种形状
const int rotateO[1][4][4] = {
  {
    {2, 2, 0, 0},
    {2, 2, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  }
};

// T 3 四种形状 
const int rotateT[4][4][4] = {
  {
    {0, 3, 0, 0},
    {3, 3, 3, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 3, 0, 0},
    {0, 3, 3, 0},
    {0, 3, 0, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 0, 0, 0},
    {3, 3, 3, 0},
    {0, 3, 0, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 3, 0, 0},
    {3, 3, 0, 0},
    {0, 3, 0, 0},
    {0, 0, 0, 0}
  }
};

// S 4 两种形状
const int rotateS[2][4][4] = {
  {
    {0, 4, 4, 0},
    {4, 4, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 4, 0, 0},
    {0, 4, 4, 0},
    {0, 0, 4, 0},
    {0, 0, 0, 0}
  }
};

// Z 5 两种形状
const int rotateZ[2][4][4] = {
  { 
    {5, 5, 0, 0},
    {0, 5, 5, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  { 
    {0, 0, 5, 0},
    {0, 5, 5, 0},
    {0, 5, 0, 0},
    {0, 0, 0, 0}
  }
};

// J 6 四种形状
const int rotateJ[4][4][4] = { 
  {
    {6, 0, 0, 0},
    {6, 6, 6, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 6, 6, 0},
    {0, 6, 0, 0},
    {0, 6, 0, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 0, 0, 0},
    {6, 6, 6, 0},
    {0, 0, 6, 0},
    {0, 0, 0, 0}
  },
  {
    {0, 6, 0, 0},
    {0, 6, 0, 0},
    {6, 6, 0, 0},
    {0, 0, 0, 0}
  }
};

// L 7 四种形状
const int rotateL[4][4][4] = {
  { 
    {0, 0, 7, 0},
    {7, 7, 7, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  { 
    {0, 7, 0, 0},
    {0, 7, 0, 0},
    {0, 7, 7, 0},
    {0, 0, 0, 0}
  },
  { 
    {0, 0, 0, 0},
    {7, 7, 7, 0},
    {7, 0, 0, 0},
    {0, 0, 0, 0}
  },
  { 
    {7, 7, 0, 0},
    {0, 7, 0, 0},
    {0, 7, 0, 0},
    {0, 0, 0, 0}
  }
};

/* 功能函数 */
// 获取 [min, max] 闭区间的随机整数
int get_random_int(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);
  return dist(get_engine());
}

// 线性计算下落间隔
int getFallInterval() {
  double seconds = pow(0.8 - (g_level - 1) * 0.007, g_level - 1);
  int ms = (int)(seconds * 1000);
  return max(50, ms);
}

// 获取按键信息
KeyAction getKey() {
  if (!_kbhit()) return KEY_NONE;
  int ch = _getch();
  if (ch == 224) { // 方向键的特殊前缀
    ch = _getch();
    switch (ch) {
      case 72: return KEY_UP;
      case 80: return KEY_DOWN;
      case 75: return KEY_LEFT;
      case 77: return KEY_RIGHT;
    }
    return KEY_NONE;
  }
  if (ch == ' ') return KEY_SPACE;
  if (ch == 'q' || ch == 'Q') return KEY_QUIT;
  return KEY_NONE;
}

void initArray() {
  for (auto& l : buffer) {
    for (auto& c : l) c = COLOR_EMPTY; 
  }
  return;
}

void output(int i, int j, string& line) {
  if (temporaryBuffer[i][j] == COLOR_EMPTY) {
    line += ' ';
  } else if (temporaryBuffer[i][j] <= 7) {
    const Color& c = COLORS_CHART[temporaryBuffer[i][j]];
    line += "\033[38;2;";
    line += to_string((int)c.r); line += ';';
    line += to_string((int)c.g); line += ';';
    line += to_string((int)c.b);
    line += "m█\033[0m";
  } else {
    const Color& c = COLORS_CHART[temporaryBuffer[i][j] / 10];
    line += "\033[38;2;";
    line += to_string((int)c.r); line += ';';
    line += to_string((int)c.g); line += ';';
    line += to_string((int)c.b);
    line += "m░\033[0m";
  }
  return;
}

string rowToString(int row) {
  string oneRow;
  for (int j = 0; j < 4; j++) {
    if (SHAPES[g_nextDice][row][j] != COLOR_EMPTY) { 
      const Color& c = COLORS_CHART[SHAPES[g_nextDice][row][j]];
      oneRow += "\033[38;2;";
      oneRow += to_string((int)c.r); oneRow += ';';
      oneRow += to_string((int)c.g); oneRow += ';';
      oneRow += to_string((int)c.b);
      oneRow += "m█\033[0m";
    } else oneRow += " ";
  }
  return oneRow;
}

void drawFrame() {
  // 改用string记录buffer内容，依次输出
  static string frame;
  frame.clear();
  frame.reserve(HIGHT * WIDTH * 24); // 预留空间，避免反复扩容
  
  // 侧边栏内容：显示引导和游戏参数（按可视行索引 0 ~ 2 * HIGHT - 1）
  string sidebar[2 * HIGHT] = {};
  sidebar[0] = "   总得分: " + to_string(g_score);
  sidebar[1] = "   当前等级: " + to_string(g_temporaryLevel);
  sidebar[2] = "   消去行数: " + to_string(g_lines);
  sidebar[4] = "  [← →]  移动";
  sidebar[5] = "  [↑]    旋转";
  sidebar[6] = "  [↓]    软降";
  sidebar[7] = "  [空格] 硬降";
  sidebar[8] = "  [Q]    退出";
  
  // 侧边栏内容：显示下一个出现的方块
  sidebar[10] = " 下一个方块";
  sidebar[11] = "  |======|";
  sidebar[12] = "  |      |";
  sidebar[13] = "  | " + rowToString(0) + " |";
  sidebar[14] = "  | " + rowToString(1) + " |";
  sidebar[15] = "  | " + rowToString(2) + " |";
  sidebar[16] = "  |======|";

  // 上方空白区
  for (int i = 0; i < HIGHT; i++) {
    for (int j = 0; j < WIDTH + 2; j++) output(i, j, frame);
    frame += sidebar[i];
    frame += '\n';
  }
  // 游戏区
  for (int i = HIGHT; i < 2 * HIGHT; i++) {
    frame += '|';
    for (int j = 1; j <= WIDTH; j++) output(i, j, frame);
    frame += '|';
    frame += '\n';
  }
  frame += '|';
  for (int i = 1; i <= WIDTH; i++) frame += '_';
  frame += '|';
  frame += '\n';
  
  // 下方空白区（此处要谨慎，因为这是在vim平台上面测试得到换行为WIDTH（10格）刚好显示不出来下次的内容)
  for (int i = 0; i < WIDTH; i++) frame += '\n';

  cout << frame; // 一次性输出整帧
}

void initBlocks() {
  g_rotateNum = 0;
  if (g_dice == -1) g_dice = get_random_int(0, 6);
  else g_dice = g_nextDice;
  g_nextDice = get_random_int(0, 6);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      temporaryBlock[i][j] = SHAPES[g_dice][i][j];
    }
  }
  g_nowCeil = HIGHT - 2;
  g_nowLeft = WIDTH / 2; 
  return;
}

// 检测虚线方块是否到底
bool isShadeBlockTouchBottom(int shadeCeil) {
  bool ok = false;
  for (int j = 0; j < 4; j++) {
    for (int i = 3; i >= 0; i--) {
      if (temporaryBlock[i][j] != COLOR_EMPTY && i + shadeCeil >= 2 * HIGHT) {
        ok = true;
        break;
      }
    }
  }
  return ok;
}

bool isShadeBlockTouchBlockBelow(int shadeCeil) {
  bool ok = false;
  for (int j = 0; j < 4; j++) {
    int nowFloor = -1;
    for (int i = 0; i < 4; i++) {
      if (temporaryBlock[i][j] != COLOR_EMPTY) nowFloor = i;
    }
    if (nowFloor != -1 && buffer[shadeCeil + nowFloor][j + g_nowLeft] != COLOR_EMPTY) ok = true;
  }
  return ok;
}

void __pushShadeBlocks(int shadeCeil) {
  for (int i = shadeCeil; i < shadeCeil + 4; i++) {
    for (int j = g_nowLeft; j < g_nowLeft + 4; j++) {
      if (buffer[i][j] == COLOR_EMPTY) temporaryBuffer[i][j] = temporaryBlock[i - shadeCeil][j - g_nowLeft] * 10;
      else temporaryBuffer[i][j] = buffer[i][j];
    }
  }
  return;
}

void pushShadeBlocks() {
  int shadeCeil = g_nowCeil;
  int maxCeil = 2 * HIGHT - 1;
  while (shadeCeil < maxCeil) {
    shadeCeil++;
    if (isShadeBlockTouchBottom(shadeCeil) || isShadeBlockTouchBlockBelow(shadeCeil)) {
      shadeCeil--;
      break;
    }
  }
  if (shadeCeil == g_nowCeil) return;
  else __pushShadeBlocks(shadeCeil);
  return;
}

void pushBlocks() {
  temporaryBuffer = buffer; 
  pushShadeBlocks();
  for (int i = g_nowCeil; i < g_nowCeil + 4; i++) {
    for (int j = g_nowLeft; j < g_nowLeft + 4; j++) {
      if (buffer[i][j] == COLOR_EMPTY) temporaryBuffer[i][j] = temporaryBlock[i - g_nowCeil][j - g_nowLeft];
      else temporaryBuffer[i][j] = buffer[i][j];
    }
  }
  return;
}

// 清除指令
// 屏幕重置
void clearScreen() {
  // cout << "\033[2J";
  cout << "\033[H";
}

// 清除行（判断行是否满、改变buffer数组、清除行）
int isRowFull(int row) {
  int cnt = 0;
  for (int j = 1; j <= WIDTH; j++) {
    if (buffer[row][j] != COLOR_EMPTY) cnt++;
  }
  return cnt;
}

void deleteRowInBuffer(int row) {
  for (; row > HIGHT; row--) {
    for (int j = 1; j <= WIDTH; j++) {
      buffer[row][j] = buffer[row - 1][j];
    }
  }
  for (int j = 1; j <= WIDTH; j++) buffer[row][j] = COLOR_EMPTY;
  return;
}

void clearRow() {
  int cleared = 0;
  int row = 2 * HIGHT - 1;
  while (row >= HIGHT) {
    if (isRowFull(row) == WIDTH) { 
      deleteRowInBuffer(row);
      cleared++;
    } else if (isRowFull(row) == 0) break;
    else row--;
  }
  g_score += baseScore[cleared] * g_temporaryLevel;
  g_lines += cleared;
  return;
}

void drawBlocks() {
  pushBlocks();
  clearScreen();
  drawFrame();
  return;
}

// 碰撞检测
bool isTouchBottom() {
  bool ok = false;
  for (int j = 0; j < 4; j++) {
    for (int i = 3; i >= 0; i--) {
      if (temporaryBlock[i][j] != COLOR_EMPTY && i + g_nowCeil >= 2 * HIGHT) {
        ok = true;
        break;
      }
    }
  }
  return ok;
}

bool isTouchLeft() {
  bool ok = false;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (temporaryBlock[i][j] != COLOR_EMPTY && buffer[i + g_nowCeil][j + g_nowLeft - 1] != COLOR_EMPTY) ok = true;
      if (ok) break;
    }
  }
  return ok;
}

bool isTouchRight() {
  bool ok = false;
  for (int i = 0; i < 4; i++) {
    for (int j = 3; j >= 0; j--) {
      if (temporaryBlock[i][j] != COLOR_EMPTY && buffer[i + g_nowCeil][j + g_nowLeft + 1] != COLOR_EMPTY) ok = true;
      if (ok) break;
    }
  }
  return ok;
}

bool isTouchBlockBelow() {
  bool ok = false;
  for (int j = 0; j < 4; j++) {
    int nowFloor = -1;
    for (int i = 0; i < 4; i++) {
      if (temporaryBlock[i][j] != COLOR_EMPTY) nowFloor = i;
    }
    if (nowFloor != -1 && buffer[g_nowCeil + nowFloor][j + g_nowLeft] != COLOR_EMPTY) ok = true;
  }
  return ok;
}

// 控制方块逻辑 
void tryMoveLeft() {
  int nowLeft = 4;
  for (int i = 0; i < 4; i++) {
    for (int j = 3; j >= 0; j--) {
      if (temporaryBlock[i][j] != COLOR_EMPTY) nowLeft = min(j, nowLeft);
    }
  }
  if (g_nowLeft + nowLeft >= 2 && !isTouchLeft()) {
    g_nowLeft--;
    drawBlocks();
  }
  return;
}

void tryMoveRight() {
  int nowRight = 0; 
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (temporaryBlock[i][j] != COLOR_EMPTY) nowRight = max(j, nowRight);
    }
  }
  if (g_nowLeft + nowRight <= 9 && !isTouchRight()) {
    g_nowLeft++;
    drawBlocks();
  }
  return;
}

// 检查方块踢墙
void checkIKickWall() {
  if (g_rotateNum == 1) return;
  if (g_nowLeft == 8) g_nowLeft--;
  else if (g_nowLeft == 9) g_nowLeft -= 2;
  else if (g_nowLeft == 0) g_nowLeft++;
  return;
}

void checkTKickWall() {
  if (g_rotateNum != 3 && g_nowLeft == 9) g_nowLeft--;
  else if (g_rotateNum != 1 && g_nowLeft == 0) g_nowLeft++;
  return;
}

void checkSOrZKickWall() {
  if (g_rotateNum == 0 && g_nowLeft == 0) g_nowLeft++;
  return;
}

void checkJOrLKickWall() {
  if ((g_rotateNum == 0 || g_rotateNum == 2) && g_nowLeft == 0) g_nowLeft++;
  else if ((g_rotateNum == 0 || g_rotateNum == 2) && g_nowLeft == 9) g_nowLeft--;
  return;
}

void tryRotate() {
  g_rotateNum++;
  int rotateLen = 0;
  switch (g_dice) {
    case 0: {
      rotateLen = 2;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateI[g_rotateNum][i][j]; 
        }
      }
      checkIKickWall();
      break;
    }
    case 1: {
      rotateLen = 1;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateO[g_rotateNum][i][j]; 
        }
      }
      break;
    } 
    case 2: {
      rotateLen = 4;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateT[g_rotateNum][i][j]; 
        }
      }
      checkTKickWall();
      break;
    } 
    case 3: {
      rotateLen = 2;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateS[g_rotateNum][i][j]; 
        }
      }
      checkSOrZKickWall();
      break;
    }
    case 4: {
      rotateLen = 2;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateZ[g_rotateNum][i][j]; 
        }
      }
      checkSOrZKickWall();
      break;
    }
    case 5: {
      rotateLen = 4;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateJ[g_rotateNum][i][j]; 
        }
      }
      checkJOrLKickWall();
      break;
    }
    case 6: {
      rotateLen = 4;
      g_rotateNum %= rotateLen;
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          temporaryBlock[i][j] = rotateL[g_rotateNum][i][j]; 
        }
      }
      checkJOrLKickWall();
      break;
    }
    default: break;
  }
  drawBlocks();
  return;
}

void accelerateFallDown() {
  g_level *= 20; 
  return;
}

void hardDrop() {
  int dropDistance = 0;
  while (true) {
    g_nowCeil++;
    if (isTouchBottom() || isTouchBlockBelow()) {
      g_nowCeil--;
      break;
    }
    dropDistance++;
  }
  drawBlocks();
  g_score += dropDistance * 2; // 硬降每格加两分
  return;
}

void blocksFallDown() { 
  /* 初始化一个方块 */
  initBlocks();
  /* 绘制每一帧的界面 */ 
  clearScreen();
  drawFrame();
  DropTimer timer(getFallInterval());

  // 按键持续检测
  while (true) {
    // 1.检测按键（非阻塞）
    KeyAction key = getKey();
    // 调试输出信息
    // if (key != KEY_NONE) {
    //   cout << "key pressed: " << key << endl;
    // }
    switch (key) {
      case KEY_LEFT: tryMoveLeft(); break;
      case KEY_RIGHT: tryMoveRight(); break;
      case KEY_UP: tryRotate(); break;
      case KEY_DOWN: {
        accelerateFallDown(); 
        timer.setInterval(getFallInterval()); // 更新计时器
        break;
      }
      case KEY_SPACE: {
        hardDrop();
        buffer = temporaryBuffer; // 立即将方块固化进buffer
        return;
      }
      case KEY_QUIT: {
        g_quit = true;
        return;
      }
      default: break;
    }

    if (key != KEY_DOWN) {
      // 每消去10行增加一级
      if (g_temporaryLevel * 10 <= g_lines) g_temporaryLevel++;
      g_level = g_temporaryLevel; // 未长按下落键，恢复原来的速度
      timer.setInterval(getFallInterval()); // 恢复原间隔 
    }
    // 2.定时下落
    if (timer.isTimeToDrop()) {
      g_nowCeil++;
      if (isTouchBottom() || isTouchBlockBelow()) break;
      drawBlocks();
      // 如果此时为软降，则加一分
      if (key == KEY_DOWN) g_score++;
    }

    // 4.短暂休眠，避免 CPU 空转
    this_thread::sleep_for(chrono::milliseconds(10));
  }
  // 更新buffer数组
  buffer = temporaryBuffer;
  return ;
}

// 判断是否结束
bool isGameOver() {
  bool ok = false;
  for (int j = 1; j <= WIDTH; j++) {
    if (buffer[HIGHT - 1][j] != COLOR_EMPTY) {
      ok = true;
      break;
    }
  }
  return ok;
}

void outputOneBlock() {
  cout << "\033[38;2;" << (int)COLORS_CHART[g_dice].r << ";" << 
  (int)COLORS_CHART[g_dice].g << ";" << (int)COLORS_CHART[g_dice].b << "m"
    << '#'
    << "\033[0m";
  return;
}

// 判断文件是否为空
bool isFileEmpty(const string& path) {
  ifstream ifs(path, ios::binary);
  if (!ifs) return true;
  return ifs.peek() == ifstream::traits_type::eof();
}

// 读入读出 
Record readRecords() {
  if (isFileEmpty(path)) return {0, 0, 0};
  ifstream ifs(path);
  Record record{0, 0, 0};
  int level, line; // 先用int读，再窄化
  if (ifs >> record.score >> level >> line) {
    record.level = static_cast<int8_t>(level);
    record.line = static_cast<int8_t>(line);
  }
  return record;
}

void writeRecords(const Record& record, const Record& history) {
  if (record.score <= history.score) return; 
  ofstream ofs(path, ios::out | ios::trunc);
  ofs << record.score << ' ' << (int)record.level << ' ' << (int)record.line << '\n';
  return;
}

void outputHistory(const Record& history) {
  cout << endl;
  cout << "======历史数据======" << endl;
  cout << "最高得分：" << history.score << endl;
  cout << "等级：" << static_cast<int>(history.level) << endl;
  cout << "消去行数：" << static_cast<int>(history.line) << endl;
  return;
}

// 通过流读写历史数据
void outputHistoryRecord() {
  int32_t score = static_cast<int32_t>(g_score);
  int8_t level = static_cast<int8_t>(g_temporaryLevel);
  int8_t line = static_cast<int8_t>(g_lines);
  Record record = { score, level, line };
  
  Record history = readRecords();

  outputHistory(history);

  writeRecords(record, history);
  
  return;
}

void drawEnd() {
  cout << "\033[2J";
  cout << "\033[H";
  int row = 8, col = 41;
  for (int i = 0; i < row; i++) {
    for (int j = 0; j < col; j++) {
      g_dice = get_random_int(1, 7);
      if (!THEEND[i][j]) {
        cout << ' ';
        continue;
      }
      outputOneBlock();
    }
    cout << endl;
  }
  cout << "总得分: " << g_score << endl;
  cout << "最终等级: " << g_temporaryLevel << endl;
  cout << "消去行数: " << g_lines << endl;
  
  outputHistoryRecord();

  return;
}

int main() {
  /* 显示设置 */
  // 隐藏光标
  // cout << "\033[?1049h";
  cout << "\033[?25l";

  /* 初始页面 */
  // 选择等级 
  // chooseLevel();

  /* 初始化界面 */
  initArray();
  drawFrame();

  /* 绘制帧界面 */
  while (!isGameOver() && !g_quit) {
    clearRow();
    drawBlocks(); // 消行之后立即刷新（重绘分数）
    blocksFallDown();
  }

  // 结束动画
  drawEnd();

  // 恢复光标
  cout << "\033[?25h";
  // cout << "\033[?1049l";

  cout << "press Enter key to exit..." << endl;
  cin.get();
  return 0;
}
