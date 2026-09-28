// 去除MIN/MAX定义
#define NOMINMAX
#include <windows.h> 
#include <iostream>
#include <chrono>

#include <string>
#include <array>

#include <vector>
#include <cstdint>

// 数学头文件
#include <cmath>
#include <algorithm>

// 存储数据流头文件以及智能指针的引入
#include <fstream>
#include <sstream>
#include <memory>
#include <iomanip>
#include <limits>
using namespace std;

/* 定义全局变量 */
// 屏幕捕获区域尺寸（电脑分辨率：2560×1600，但是还要加上副屏，而副屏的物理大小与主屏一样，注意偏移（原大小的1.5倍是主屏，但是物理显示上被拓展了））
// 黑框左上角位置为(3610, 320)，右下角为(4110, 1250)
const int CAPTURE_WIDTH  = 500;
const int CAPTURE_HEIGHT = 1065;
const int BYTES_PER_PIXEL = 4; // 像素格式为 BGRA，每个像素 4 个字节
const int FRAME_SIZE = CAPTURE_WIDTH * CAPTURE_HEIGHT * BYTES_PER_PIXEL; // 一帧的字节大小
const int MAX_DIST2 = 60 * 60 * 3; // 定义三色距离阈值
const int TOL = 20; // 方框内不需要显示下落的过程，此处定义这个阈值实现getColor
const int TARGETLEFT = 3610; // 定义截取屏幕左上角的位置
const int TARGETTOP = 225;
const int FRAMEOUTPUT = 30; // 定义为每FRAMEOUTPUT次就输出一次帧信息
// 屏幕采样点位置定义
const int SAMPLE_ORIGIN_X = 23; // x方向起始位置
const int SAMPLE_ORIGIN_Y = 115; // y方向起始位置
const int SAMPLE_STEP = 46; // 采样步长

// 写入数据路径
const string path = "gene_read.txt";
// 日志
static ofstream g_log("control_log.txt", ios::out | ios::trunc);

// 定义宏变量
static int constexpr WIDTH = 10;
static int constexpr HIGHT = 20;
static int constexpr INITLEFT = 4;
static int constexpr INITCEIL = HIGHT - 2;

// 定义参数
uint8_t g_nowCeil = INITCEIL, g_nowLeft = INITLEFT; // 设置方块（4 × 4）的左上角坐标
uint8_t g_color = 0;
array<array<char, WIDTH + 5>, 2 * HIGHT + 5> g_buffer = {};
int temporaryBlock[4][4] = {
  {0, 0, 0, 0}, 
  {0, 0, 0, 0}, 
  {0, 0, 0, 0}, 
  {0, 0, 0, 0}, 
};

// FFmpeg 进程句柄
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;
PROCESS_INFORMATION g_pi = { 0 };

// 定义方块代码
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
    {0, 2, 2, 0}, 
    {0, 2, 2, 0},
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
// I 1 四种形状（这里要很小心，别人的逻辑下这个方块是按照4*4的中心来旋转的）
const int rotateI[4][4][4] = {
  {
    {0, 0, 0, 0},
    {1, 1, 1, 1},
    {0, 0, 0, 0},
    {0, 0, 0, 0}
  },
  { 
    {0, 0, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 0}
  },
  { 
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {1, 1, 1, 1},
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
    {0, 2, 2, 0},
    {0, 2, 2, 0},
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

// 定义结构体
// 定义颜色构成
struct Color {
  uint8_t r, g, b;
};

// 引入四个特征
struct Features {
  double linesCleared; // 本次消行数
  double aggregateHeight; // 各列高度之和
  double holes; // 空洞数
  double bumpiness; // 相邻列高度差绝对值之和
};

// 定义放置位置
struct Placements {
  uint8_t left;
  uint8_t ceil;
  uint8_t rotate;
  Features features; 
};

// 定义下落参数
struct DropResult {
  int ceil;
  int linesCleared;
};

// 定义rgb三色代码
// 定义方块在空中（即缓存区）时亮色的颜色
const Color COLORS_LIGHT_CHART[] = {
  {41, 227, 158}, // cyan I
  {228, 190, 41}, // yellow O
  {207, 62, 193}, // pink T
  {134, 183, 51}, // green S
  {228, 43, 53}, // red Z 
  {80, 58, 206}, // purple J
  {229, 113, 42} // orange L
};

// 定义方块落下后暗色的颜色
const Color COLORS_DARK_CHART[] = {
  {50, 182, 135}, // cyan
  {184, 158, 50}, // yellow
  {166, 62, 155}, // pink
  {134, 183, 51}, // green
  {190, 53, 65}, // red
  {80, 62, 164}, // purple
  {182, 101, 52} // orange
};

// 定义枚举类型
// 设置颜色代码
enum TetrisColor {
	COLOR_EMPTY = 0, // 用0表示空格
  COLOR_CYAN, // 青色
  COLOR_YELLOW, // 黄色
  COLOR_PINK, // 粉色
  COLOR_GREEN, // 绿色
	COLOR_RED, // 红色
  COLOR_PURPLE, // 紫色
  COLOR_ORANGE // 橘黄色
};

/* 定义遗传类类型 */
class BlockAI {
public: 
	virtual ~BlockAI() = default;
	
	// 核心接口：给一个落点特征，返回评分（越大越好）
	virtual double evaluate(const Features& f) const = 0;
	
	// 遗传算法接口
	virtual vector<double> getGenes() const = 0;
	virtual void setGenes(const vector<double>& genes) = 0;
	virtual int geneCount() const = 0;

	// 克隆（因为Individual里存unique_ptr) 
	virtual unique_ptr<BlockAI> clone() const = 0;
	
	// 方块类型0~6
	virtual int type() const = 0;
};

// 7个派生类，每个类独立权重，目前公式想同，可在任意类中修改evaluate
#define DEFINE_BLOCK_AI(NAME, TYPE_ID)                \
class NAME : public BlockAI {                         \
	array<double, 4> w_{};                              \
public:                                               \
	NAME() = default;                                   \
	explicit NAME(const array<double, 4>& w) : w_(w) {} \
	                                                    \
	double evaluate(const Features& f) const override { \
		return w_[0] * f.linesCleared                     \
 				 + w_[1] * f.aggregateHeight                  \
			   + w_[2] * f.holes                            \
				 + w_[3] * f.bumpiness;                       \
	}                                                   \
	vector<double> getGenes() const override {          \
		return { w_[0], w_[1], w_[2], w_[3] };            \
	}                                                   \
	void setGenes(const vector<double>& g) override {   \
		for (int i = 0; i < 4 && i < (int)g.size(); ++i) w_[i] = g[i]; \
	}                                                   \
	int geneCount() const override { return 4; }        \
	unique_ptr<BlockAI> clone() const override {        \
		return make_unique<NAME>(*this);                  \
	}                                                   \
	int type() const override { return TYPE_ID; }       \
};

// 初始化7个派生类
DEFINE_BLOCK_AI(IBlockAI, 0);
DEFINE_BLOCK_AI(OBlockAI, 1);
DEFINE_BLOCK_AI(TBlockAI, 2);
DEFINE_BLOCK_AI(SBlockAI, 3);
DEFINE_BLOCK_AI(ZBlockAI, 4);
DEFINE_BLOCK_AI(JBlockAI, 5);
DEFINE_BLOCK_AI(LBlockAI, 6);

// 工厂：根据类型创建对应的AI
unique_ptr<BlockAI> makeAI(int type) {
  switch (type) {
    case 0: return make_unique<IBlockAI>();
    case 1: return make_unique<OBlockAI>();
    case 2: return make_unique<TBlockAI>();
    case 3: return make_unique<SBlockAI>();
    case 4: return make_unique<ZBlockAI>();
    case 5: return make_unique<JBlockAI>();
    case 6: return make_unique<LBlockAI>();
  }
  return nullptr;
}

/* 存储一张照片 */
void SaveFrameAsBMP(const vector<uint8_t>& buffer, int width, int height, const char* filename) {
  const int rowSize = width * 4;
  const int dataSize = rowSize * height;
  const int fileSize = 54 + dataSize;

  ofstream f(filename, ios::binary);
  if (!f) return;

  uint8_t header[54] = {0};
  // BITMAPFILEHEADER
  header[0] = 'B'; header[1] = 'M';
  *(int*)&header[2] = fileSize;
  *(int*)&header[10] = 54;
  // BITMAPINFOHEADER
  *(int*)&header[14] = 40;
  *(int*)&header[18] = width;
  *(int*)&header[22] = -height;
  *(int*)&header[26] = 1;
  *(short*)&header[28] = 32;
  *(int*)&header[34] = dataSize;

  f.write((char*)header, 54);
  f.write((char*)buffer.data(), dataSize);
}

/* ===== 读入文件部分 ===== */ 
// 日志
void Log(const string& s) {
  if (g_log.is_open()) {
    g_log << s << endl;
    g_log.flush(); // 立即写入，避免崩溃时丢失数据
  }
}

bool isFileEmpty() {
  ifstream ifs(path, ios::binary);
  if (!ifs) return true;
  return ifs.peek() == ifstream::traits_type::eof();
}

// 判断读入文件的行数
int countValidLines() {
  ifstream in(path);
  if (!in) return -1;

  int lines = 0; 
  string line;
  while (getline(in, line)) {
    if (line.empty()) continue; // 跳过空行
    if (line[0] == '#' || line[0] == '[') continue; // 跳过注释/标记
    lines++;
  }
  return lines;
}

// 改为载入最好的一次成绩
bool loadBestGroup(vector<double>& genes, double& bestFitness) {
  ifstream in(path);
  if (!in) return false;

  vector<double> current;
  vector<double> best;

  double currentFitness = -numeric_limits<double>::infinity();
  bestFitness = -numeric_limits<double>::infinity();

  auto finishCurrent = [&]() {
    if (current.size() == 28 && currentFitness > bestFitness) {
      bestFitness = currentFitness;
      best = current;
    }
  };

  string line;
  while (getline(in, line)) {
    if (line.empty()) continue;

    if (line[0] == '#' || line[0] == '[') {
      finishCurrent();
      current.clear();
      currentFitness = -numeric_limits<double>::infinity();

      auto pos = line.find("fitness=");
      if (pos != string::npos) {
        istringstream iss(line.substr(pos + 8));
        iss >> currentFitness;
      }
      continue;
    }

    for (char& c : line) {
      if (c == ',') c = ' ';
    }

    istringstream iss(line);
    double value; 
    while (iss >> value) current.push_back(value);
  }
  
  finishCurrent();

  if (best.size() != 28) return false;

  genes = best;
  return true;
}

/* ===== 遗传算法部分 ===== */
// 计算特征值
Features calcFeatures(const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, int linesCleared) {
  array<int, WIDTH> heights{};
  // 计算各列高度
  for (int j = 1; j < WIDTH + 1; ++j) {
    for (int i = HIGHT; i < 2 * HIGHT; ++i) {
      if (nextBuffer[i][j] != 0) {
        heights[j - 1] = 2 * HIGHT - i; 
        break;
      }
    }
  }
  
  // 计算各列高度之和
  double agg = 0;
  for (int h : heights) agg += h;

  // 计算共有多少空洞
  double holes = 0;
  for (int j = 1; j < WIDTH + 1; ++j) {
    bool seen = false;
    for (int i = HIGHT; i < 2 * HIGHT; ++i) {
      if (nextBuffer[i][j] != 0) seen = true;
      else if (seen) holes++;
    }
  }

  // 计算各列高度差的绝对值之和
  double bump = 0;
  for (int j = 0; j + 1 < WIDTH; ++j) bump += abs(heights[j] - heights[j + 1]);

  return { (double)linesCleared, agg, holes, bump };
}

// =============================
// 遗传算法
// =============================
struct Individual {
  array<unique_ptr<BlockAI>, 7> ais;
  double fitness = 0;

  Individual() {
    for (int i = 0; i < 7; i++) ais[i] = makeAI(i);
  }

  // unique_ptr 不可拷贝，禁用拷贝，启用移动
  Individual(const Individual&) = delete;
  Individual& operator=(const Individual&) = delete;
  Individual(Individual&&) = default;
  Individual& operator=(Individual&&) = default;

  // 深拷贝（用于精英保留、选择）
  Individual deepClone() const {
    Individual c;
    for (int i = 0; i < 7; ++i) c.ais[i] = ais[i]->clone();
    c.fitness = fitness;
    return c;
  }

  // 将所有类型的权重拼成一个向量，方便交叉/变异
  vector<double> allGenes() const {
    vector<double> g;
    for (const auto& ai : ais) {
      auto part = ai->getGenes();
      g.insert(g.end(), part.begin(), part.end());
    }
    return g;
  }

  void setAllGenes(const vector<double>& g) {
    int idx = 0;
    for (auto& ai : ais) {
      int n = ai->geneCount();
      vector<double> part(g.begin() + idx, g.begin() + idx + n);
      ai->setGenes(part);
      idx += n;
    }
  }
};

/* ===== FFmpeg部分 ===== */ 
// 启动 FFmpeg 进程，并重定向其标准输出到管道
bool StartFFmpegCapture() {
  SECURITY_ATTRIBUTES sa = { sizeof(sa) };
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  // 创建管道
  if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sa, 0)) {
    std::cerr << "CreatePipe failed: " << GetLastError() << std::endl;
    return false;
  }
  // 读取端不被子进程继承
  SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

  // 用 std::wstring 逐步拼命令，避免指针 + wstring 混淆
  std::wstring cmd;

  // 注意：这里用双引号把 ffmpeg 路径括起来，防止路径含空格
  cmd += L"\"D:\\ffmpeg\\ffmpeg-master-latest-win64-gpl-shared\\bin\\ffmpeg.exe\" ";
  cmd += L"-f gdigrab -framerate 30 -i desktop ";
  // crop 参数不要加引号，Windows 会原样传给 ffmpeg
  cmd += L"-vf crop=";
  cmd += to_wstring(CAPTURE_WIDTH);
  cmd += L":";
  cmd += to_wstring(CAPTURE_HEIGHT);
 
  // 拼接crop坐标
  cmd += L":";
  cmd += to_wstring(TARGETLEFT);
  cmd += L":";
  cmd += to_wstring(TARGETTOP);
  cmd += L" ";
   
  cmd += L"-f rawvideo -pix_fmt bgra -";

  STARTUPINFOW si = { sizeof(si) };
  si.hStdOutput = g_hChildStd_OUT_Wr;
  si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;

  ZeroMemory(&g_pi, sizeof(g_pi));

  if (!CreateProcessW(
      NULL,
      &cmd[0],           // 命令行（必须可写）
      NULL, NULL,
      TRUE,              // 句柄继承
      CREATE_NO_WINDOW,
      NULL, NULL,
      &si, &g_pi)) {
    std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_OUT_Wr);
    g_hChildStd_OUT_Rd = NULL;
    g_hChildStd_OUT_Wr = NULL;
    return false;
  }

  // 父进程不需要写端
  CloseHandle(g_hChildStd_OUT_Wr);
  g_hChildStd_OUT_Wr = NULL;

  std::cout << "FFmpeg started, PID: " << g_pi.dwProcessId << std::endl;
  return true;
}

// 从管道中读取一帧原始像素数据
bool ReadFrameFromPipe(std::vector<uint8_t>& frameBuffer) {
  frameBuffer.resize(FRAME_SIZE);
  DWORD totalRead = 0;

  while (totalRead < (DWORD)FRAME_SIZE) {
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(
      g_hChildStd_OUT_Rd,
      frameBuffer.data() + totalRead,
      FRAME_SIZE - totalRead,
      &bytesRead,
      NULL);

    if (!ok) {
      std::cerr << "ReadFile failed: " << GetLastError() << std::endl;
      return false;
    }  
    if (bytesRead == 0) {
      // 管道关闭，FFmpeg 可能已退出
      return false;
    }
    totalRead += bytesRead;
  }
  return true;
}

// 停止录屏进程
void StopFFmpegCapture() {
    if (g_pi.hProcess) {
      TerminateProcess(g_pi.hProcess, 0);
      WaitForSingleObject(g_pi.hProcess, 1000);
      CloseHandle(g_pi.hProcess);
      CloseHandle(g_pi.hThread);
      g_pi.hProcess = NULL;
      g_pi.hThread = NULL;
    }
    if (g_hChildStd_OUT_Rd) {
      CloseHandle(g_hChildStd_OUT_Rd);
      g_hChildStd_OUT_Rd = NULL;
    }
    if (g_hChildStd_OUT_Wr) {
      CloseHandle(g_hChildStd_OUT_Wr);
      g_hChildStd_OUT_Wr = NULL;
    }
}

// 从 buffer 里取指定坐标的像素颜色
COLORREF GetPixelFromBuffer(const std::vector<uint8_t>& buffer, int x, int y) {
  int index = (y * CAPTURE_WIDTH + x) * BYTES_PER_PIXEL;
  uint8_t b = buffer[index];
  uint8_t g = buffer[index + 1];
  uint8_t r = buffer[index + 2];
  return RGB(r, g, b);
}

/* 屏幕显示函数 */
// 屏幕重置
void clearScreen() {
  cout << "\033[H";
}

// 显示结果
void output(int i, int j, string& line) {
  if (g_buffer[i][j] == COLOR_EMPTY) {
    line += ' ';
  } else {
    const Color& c = (i < HIGHT) ? COLORS_LIGHT_CHART[g_buffer[i][j] - 1] : COLORS_DARK_CHART[g_buffer[i][j] - 1];
    line += "\033[38;2;";
    line += to_string((int)c.r); line += ';';
    line += to_string((int)c.g); line += ';';
    line += to_string((int)c.b);
    line += "m█\033[0m";
  } 
	return;
}

// 绘制框架
void drawFrame() {
  // 改用string记录buffer内容，依次输出
  static string frame;
  frame.clear();
  frame.reserve(HIGHT * WIDTH * 24); // 预留空间，避免反复扩容
  
  // 上方空白区
  for (int i = 0; i < HIGHT; i++) {
    for (int j = 0; j < WIDTH + 2; j++) output(i, j, frame);
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

/* ===== 模拟按键输入 ===== */
// 按下并松开一个键（虚拟键码）
void SendKey(WORD vk) {
	INPUT in[2] = {};
	in[0].type = INPUT_KEYBOARD;
  in[0].ki.wVk = vk;
  in[1].type = INPUT_KEYBOARD;
  in[1].ki.wVk = vk;
  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, in, sizeof(INPUT));
}

// 按下并松开一个键（扫描码，兼容游戏）
void SendKeyScan(WORD vk) {
  WORD vsc = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);

  DWORD downFlag = KEYEVENTF_SCANCODE;
  DWORD upFlag = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

  if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT) {
    downFlag |= KEYEVENTF_EXTENDEDKEY;
    upFlag |= KEYEVENTF_EXTENDEDKEY;
  }

  INPUT in[2] = {};
  in[0].type = INPUT_KEYBOARD;
  in[0].ki.wScan = vsc;
  in[0].ki.dwFlags = downFlag;
  in[1].type = INPUT_KEYBOARD;
  in[1].ki.wScan = vsc;
  in[1].ki.dwFlags = upFlag;
  SendInput(2, in, sizeof(INPUT));
}

// 添加按键延迟以增加识别率
void SendKeyScanDelay(WORD vk, int ms = 10) {
  // 写入日志
  // 记录按键名
  const char* name = "?";
  switch(vk) {
    case VK_LEFT:   name = "LEFT"; break;
    case VK_RIGHT:  name = "RIGHT"; break;
    case VK_UP:     name = "UP(rotate)"; break;
    case VK_SPACE:  name = "SPACE(hardDrop)"; break;
  }
  Log(string("   key ") + name + " delay=" + to_string(ms) + "ms");

	SendKeyScan(vk);
	Sleep(ms);
}

// 平均值取样判色
Color sampleAvgColor(const vector<uint8_t>& frameBuffer, int cx, int cy, int r = 1) {
  long sumR = 0, sumG = 0, sumB = 0;
  int count = 0;

  for (int dy = -r; dy <= r; ++dy) {
    for (int dx = -r; dx <= r; ++dx) {
      int x = cx + dx;
      int y = cy + dy; 

      // 边界保护
      if (x < 0 || x >= CAPTURE_WIDTH) continue;
      if (y < 0 || y >= CAPTURE_HEIGHT) continue;
      COLORREF c = GetPixelFromBuffer(frameBuffer, x, y);
      sumR += GetRValue(c);
      sumG += GetGValue(c);
      sumB += GetBValue(c);
      ++count;
    }
  }
  if (count == 0) return { 0, 0, 0 };
  return {
    (uint8_t)(sumR / count),
    (uint8_t)(sumG / count),
    (uint8_t)(sumB / count)
  };
}

/* ===== 颜色处理部分 ===== */
// 获取目标坐标下的颜色（改为获取RGB距离最小的那个颜色）
// 需要说明的是这个方法确实准确，但是计算量太大导致太卡，而且读取buffer的时候不能读入下落中的方块
int getTopColor(const vector<uint8_t>& frameBuffer, int x, int y) {
  // **注意：GetPixelFromBuffer的坐标是相对于缓存区，而不是屏幕坐标
  Color c = sampleAvgColor(frameBuffer, x, y);

  // 去除背景（暗色）干扰
  int mx = max({c.r, c.g, c.b});
  int mn = min({c.r, c.g, c.b});

  // 饱和度太低（灰/黑/白）-> 背景
  if (mx - mn < 40) return COLOR_EMPTY;

  int bestIdx = -1;
  int bestDist = INT_MAX;
  for (int i = 0; i < 7; ++i) {
    int dr = (int)c.r - COLORS_LIGHT_CHART[i].r;
    int dg = (int)c.g - COLORS_LIGHT_CHART[i].g;
    int db = (int)c.b - COLORS_LIGHT_CHART[i].b;
    int dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      bestIdx = i;
    }
  }

  // 距离太远就认为是空
  if (bestDist > MAX_DIST2) return COLOR_EMPTY;

  return bestIdx + 1; // 从COLOR_RED开始
}

// 使用格子投票的方法来判定颜色
int getColorMajority(const vector<uint8_t>& frameBuffer, int cx, int cy) {
	int votes[8] = {0};
	const int N = 5; // 选取待分析点周围5×5的像素点进行分析
  const int SPACING = 6; // 设置选取点的步长

  for (int dy = -N / 2; dy <= N / 2; ++dy) {
    for (int dx = -N / 2; dx <= N / 2; ++dx) {
      int sx = cx + dx * SPACING; 
      int sy = cy + dy * SPACING;
      int c = getTopColor(frameBuffer, sx, sy);
      votes[c]++;
    }
  }

  // 找众数（EMPTY也参与投票）
  int bestColor = COLOR_EMPTY, bestCount = votes[COLOR_EMPTY];
  for (int i = 1; i <= 7; ++i) {
    if (votes[i] > bestCount) {
      bestCount = votes[i]; 
      bestColor = i;
    }
  }

  // 要求占比超过一半，否则当空
  int total = N * N;
  if (bestCount * 2 <= total) return COLOR_EMPTY;
  return bestColor;
}

// 使用比较法判定颜色，此时不会输出下落的方块
int getColor(const vector<uint8_t>& frameBuffer, int x, int y) {
  // **注意：GetPixelFromBuffer的坐标是相对于缓存区，而不是屏幕坐标
  Color c = sampleAvgColor(frameBuffer, x, y);
  uint8_t r = c.r;
  uint8_t g = c.g;
  uint8_t b = c.b;

  // 辅助信息 
  // cout << "detect rgb = " << r << "," << g << "," << b << " at buf(" << row << "," << col << ")" << endl;
  
  // 判断颜色
  // cyan
  if ((int)abs(r - COLORS_DARK_CHART[0].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[0].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[0].b) <= TOL) return COLOR_CYAN;

  // yellow
  else if ((int)abs(r - COLORS_DARK_CHART[1].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[1].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[1].b) <= TOL) return COLOR_YELLOW;
  
  // pink
  else if ((int)abs(r - COLORS_DARK_CHART[2].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[2].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[2].b) <= TOL) return COLOR_PINK;

  // green
  else if ((int)abs(r - COLORS_DARK_CHART[3].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[3].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[3].b) <= TOL) return COLOR_GREEN;

  // red
  else if ((int)abs(r - COLORS_DARK_CHART[4].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[4].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[4].b) <= TOL) return COLOR_RED;

  // purple
  else if ((int)abs(r - COLORS_DARK_CHART[5].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[5].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[5].b) <= TOL) return COLOR_PURPLE;

  // orange
  else if ((int)abs(r - COLORS_DARK_CHART[6].r) <= TOL && (int)abs(g - COLORS_DARK_CHART[6].g) <= TOL && (int)abs(b - COLORS_DARK_CHART[6].b) <= TOL) return COLOR_ORANGE;

  // blank
  else return COLOR_EMPTY;
}

/* ===== 模拟下落并找到最佳位置 ===== */
// 判定是否触底
Placements compareScore(const array<unique_ptr<BlockAI>, 7>& ais, Placements bestScore, int left, int ceil, int type, const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, int linesCleared) {
  // 复制一份（不允许窄化转换）
  Placements nowScore{
    static_cast<uint8_t>(left),
    static_cast<uint8_t>(ceil),
    static_cast<uint8_t>(type),
    calcFeatures(nextBuffer, linesCleared)
  };

  double s = ais[g_color - 1]->evaluate(nowScore.features);
  if (bestScore.left == numeric_limits<uint8_t>::max()) bestScore = nowScore;
  else {
    double best = ais[g_color - 1]->evaluate(bestScore.features);
    bestScore = s > best ? nowScore : bestScore;
  }
  return bestScore;
}

bool simulateIsTouchBottom(int ceil) {
  bool ok = false;
	for (int j = 0; j < 4; ++j) {
    int floor = 0; 
    for (int i = 0; i < 4; ++i) {
      if (temporaryBlock[i][j] != COLOR_EMPTY) floor = i + 1;
    }
    if (ceil + floor >= 2 * HIGHT) {
      ok = true;
      break;
    }
  }
  return ok;
}

bool simulateIsTouchBlockBelow(const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& temporaryBuffer, int ceil, int left) {
  bool ok = false;
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 4; ++i) {
      if (temporaryBlock[i][j] != COLOR_EMPTY && temporaryBuffer[ceil + i + 1][left + j] != COLOR_EMPTY) {
        ok = true;
        break;
      }
    }
  }
  return ok;      
}

// 模拟下落后的buffer
DropResult simulateNextBuffer(array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& temporaryBuffer, int left) {
	nextBuffer = temporaryBuffer;
  int ceil = g_nowCeil;
  int linesCleared = 0;
  while (true) {
    ceil++;
    if (simulateIsTouchBottom(ceil) || simulateIsTouchBlockBelow(temporaryBuffer, ceil, left)) break;
  }
  
  // 将temporaryBlock中的方块通过计算得到的ceil值压入nextBuffer数组中
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (nextBuffer[ceil + i][left + j] != COLOR_EMPTY) continue;
      nextBuffer[ceil + i][left + j] = temporaryBlock[i][j];
    }
  }

  // 计算此时消去的行数
  for (int i = 2 * HIGHT - 1; i >= HIGHT; --i) {
    bool full = true;
    for (int j = 1; j <= WIDTH; ++j) {
      if (nextBuffer[i][j] == COLOR_EMPTY) {
        full = false;
        break;
      } 
    }
    if (full) {
      for (int k = i; k > HIGHT; --k) nextBuffer[k] = nextBuffer[k - 1];
      for (int j = 1; j <= WIDTH; ++j) nextBuffer[HIGHT][j] = COLOR_EMPTY;
      linesCleared++;
      i++;
    }
  }
  
  // 记录此时的g_nowceil填入nowScore中
  return {ceil, linesCleared};
} 

// 模拟按键操作将方块放置到合适的位置
void placeBlocks(int left, int rotate) {
  // cout << "place: left=" << left << " rotate=" << rotate << endl;
  // 执行旋转操作
  while (rotate) SendKeyScanDelay(VK_UP), rotate--;

  // 执行左右移动
  if (left < INITLEFT) {
    for (; left < INITLEFT; ++left) SendKeyScanDelay(VK_LEFT);
  } else {
    for (; left > INITLEFT; --left) SendKeyScanDelay(VK_RIGHT);
  }

	SendKeyScanDelay(VK_SPACE);
  return;
}

// 向temporaryBlock压入执行每次旋转时压入旋转后的图形
void pushTemporaryBlock(int idx) {
  switch (g_color) {
    case 1: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateI[idx][i][j];
      }
      break;
    }
    case 2: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateO[idx][i][j];
      }
      break;
    }
    case 3: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateT[idx][i][j];
      }
      break;
    }
    case 4: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateS[idx][i][j];
      }
      break;
    }
    case 5: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateZ[idx][i][j];
      }
      break;
    }
    case 6: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateJ[idx][i][j];
      }
      break;
    }
    case 7: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateL[idx][i][j];
      }
      break;
    }
  }

  return;
}

void tryAllRotateShapes(const array<unique_ptr<BlockAI>, 7>& ais, const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& temporaryBuffer) {
  // 未识别出方块，不发按键
  if (g_color < 1 || g_color > 7) return;

  Placements bestScore;
  // 初始化bestScore中的left为最大值
  bestScore.left = numeric_limits<uint8_t>::max();

  int rotateLen = 0;

  int left = 0, right = 0; // 定义左右边界

  // 定义ceil即记录得到的最低顶部值，定义linesCleared即记录得到的消行数
  int ceil = 0, linesCleared = 0;
  
  array<array<char, WIDTH + 5>, 2 * HIGHT + 5> nextBuffer = {};
  switch (g_color - 1) {
    case 0: {
      rotateLen = 4;
      for (int type = 0; type < rotateLen; type++) {
        pushTemporaryBlock(type);
        if (type == 0 || type == 3) left = 1, right = WIDTH - 2;
        else if (type == 1) left = -1, right = WIDTH - 1;
        else left = 0, right = WIDTH;
        for (; left < right; ++left) {
          auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, temporaryBuffer, left);
          bestScore = compareScore(ais, bestScore, left, ceil, type, nextBuffer, linesCleared);
        }
      }
      pushTemporaryBlock(bestScore.rotate);
      break;
    }
    case 1: {
      pushTemporaryBlock(0);
      for (int left = 0; left < WIDTH - 1; ++left) {
        auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, temporaryBuffer, left);
        bestScore = compareScore(ais, bestScore, left, ceil, 0, nextBuffer, linesCleared);
      }
      pushTemporaryBlock(0);
      break;
    }
    case 5:
    case 6:
    case 2: {
      rotateLen = 4;
      for (int type = 0; type < rotateLen; type++) {
        pushTemporaryBlock(type);
        if (type == 0 || type == 2) left = 1, right = WIDTH - 1;
        else if (type == 1) left = 0, right = WIDTH - 1;
        else left = 1, right = WIDTH;
        for (; left < right; ++left) {
          auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, temporaryBuffer, left);
          bestScore = compareScore(ais, bestScore, left, ceil, type, nextBuffer, linesCleared);
        }
      }
      pushTemporaryBlock(bestScore.rotate);
      break;
    } 
    case 3: 
    case 4: {
      rotateLen = 2;
      for (int type = 0; type < rotateLen; type++) {
        pushTemporaryBlock(type);
        if (type == 0) left = 1, right = WIDTH - 1;
        else left = 0, right = WIDTH - 1;
        for (; left < right; ++left) {
          auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, temporaryBuffer, left);
          bestScore = compareScore(ais, bestScore, left, ceil, type, nextBuffer, linesCleared);
        }
      }
      pushTemporaryBlock(bestScore.rotate);
      break;
    }
    default: break;
  }
  placeBlocks(bestScore.left, bestScore.rotate);
  return;
}

void initBlocks() {
  g_nowCeil = INITCEIL;
  g_nowLeft = INITLEFT;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = COLOR_EMPTY;
  }
  g_color = COLOR_EMPTY;
}

// 边缘检测
void checkEdge(const vector<uint8_t>& frameBuffer, const array<unique_ptr<BlockAI>, 7>& ais, const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& temporaryBuffer) {
  // 静态变量，跨帧保持
  static int lastColor = COLOR_EMPTY;  

  // 检测缓存区的方块颜色
  int curColor = getColorMajority(frameBuffer, SAMPLE_ORIGIN_X + 4 * SAMPLE_STEP, SAMPLE_ORIGIN_X + SAMPLE_STEP);

  if (curColor != COLOR_EMPTY && curColor != lastColor) tryAllRotateShapes(ais, temporaryBuffer);
   
  lastColor = curColor;
}

// 获取每一帧的图像 
void getFrame(const array<unique_ptr<BlockAI>, 7>& ais) { 
	// 初始化方块
  initBlocks();

  vector<uint8_t> frameBuffer;
  int frameCount = 0;

  // 定义一个临时数组来储存当前帧的颜色矩阵
  array<array<char, WIDTH + 5>, 2 * HIGHT + 5> temporaryBuffer = {};

  // 持续读入
  while (true) {
    if (!ReadFrameFromPipe(frameBuffer)) {
      cerr << "Read frame failed, ffmpeg may have exited." << endl;
      break;
    }

    // 依次获取屏幕上对象点颜色
    // （改为）只获取缓存区内其中一个等待下落的方块是什么
    temporaryBuffer[HIGHT - 1][4 + 1] = getColorMajority(frameBuffer, SAMPLE_ORIGIN_X + 4 * SAMPLE_STEP, SAMPLE_ORIGIN_X + SAMPLE_STEP);

    // 读取此时缓存区中的方块颜色
    g_color = temporaryBuffer[HIGHT - 1][4 + 1];

    // 获取框内的方块
    for (int row = 0; row < HIGHT; ++row) {
      for (int col = 0; col < WIDTH; ++col) temporaryBuffer[row + HIGHT][col + 1] = getColor(frameBuffer, SAMPLE_ORIGIN_X + col * SAMPLE_STEP, SAMPLE_ORIGIN_Y + row * SAMPLE_STEP);
    }
    
    // 边界判定
    checkEdge(frameBuffer, ais, temporaryBuffer);

    // 判断temporaryBuffer是否与g_buffer相等，若相等则更新数组并显示
    if (temporaryBuffer != g_buffer) {
      g_buffer = temporaryBuffer;
      clearScreen();
      drawFrame();
    }
    // 增加帧计数
    frameCount++;
    
    // 检测到第一帧的时候存一次图片（调试用）
    if (frameCount == 1) {
      SaveFrameAsBMP(frameBuffer, CAPTURE_WIDTH, CAPTURE_HEIGHT, "capture.bmp");
      cout << "saved capture.bmp" << endl;
    }
    // 每FRAMEOUTPUT输出一次

    // if (frameCount % FRAMEOUTPUT == 0) {
    //   std::cout << "Processed " << frameCount << " frames..." << std::endl;
    // }
  }
}

/* ===== 游戏执行部分 ===== */
// 入口层
void autoPlay() {
  // 读入数据
  vector<double> genes;
  double historicalBest = 0.0;
  bool loaded = loadBestGroup(genes, historicalBest);
	
	// 判断是否成功读入数据
 	if (!loaded) {
		cerr << "loadBestGroup failed, check gene_read.txt\n";
		return;
	}	

  // 载入基因
  Individual ind;
  ind.setAllGenes(genes);

  getFrame(ind.ais);

  return;
}

int main() {
  // 初始化并设置图像获取相关参数
  SetProcessDPIAware(); // 避免 DPI 缩放导致坐标问题

  // 启动并检查是否启动捕获
  if (!StartFFmpegCapture()) {
    return 1;
  }
  
  // 调用入口层函数
  autoPlay();
  
  // 停止获取帧
  StopFFmpegCapture();
  return 0;
}
