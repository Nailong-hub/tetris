#include <iostream> 
#include <string>
#include <array>
#include <random>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cmath>
#include <algorithm>

// Keyboard工具类
#include <conio.h>
#include <windows.h> // 如果需要 Sleep

// 流式文件设置及智能指针的引入
#include <memory>
#include <fstream> 
#include <sstream>
#include <iomanip> // 设置化流对象输出格式
#include <limits> // 方便查找输出精度

using namespace std;

// 定义宏变量
static int constexpr WIDTH = 10;
static int constexpr HIGHT = 20;

// 定义遗传有关的宏变量
static int constexpr POP = 30;
static int constexpr GENS = 20;
static int constexpr ELITE = 3;
static double constexpr MUT_RATE = 0.2;

// 定义渲染开关
static bool g_render = true;

// 定义自动控制/训练开关
static bool g_train = false;

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

// 调试：设置手工种子
vector<double> seedGenes = {
  3.0, -0.5, -2.0, -0.3, // I
  3.0, -0.5, -2.0, -0.3, // O
  3.0, -0.5, -2.0, -0.3, // T
  3.0, -0.5, -2.0, -0.3, // S
  3.0, -0.5, -2.0, -0.3, // Z
  3.0, -0.5, -2.0, -0.3, // J
  3.0, -0.5, -2.0, -0.3, // L
};

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
 
const string path = "gene_progress.txt";
const string log_path = "log.txt";

static const int baseScore[] = {0, 100, 300, 500, 800}; // 标准基础分

// 全局静态引擎（随机数，只初始化一次）
static mt19937& get_engine() {
  static mt19937 engine(std::random_device{}());
  return engine;
}

// 定义结构体
struct Color {
  uint8_t r, g, b;
};

// 目前引入四个特征
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

// 定义模拟对对应的方块进行操作的虚类和派生类
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

// =============================
// 读入读出数据
// =============================
// 判断文件是否为空
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

// 追加写入
void appendGenes(const vector<double>& genes, double fitness) {
  ofstream out(path, ios::app); // 追加而不是清空
  if (!out) return;
  
  int iter = countValidLines() / 7;

  out << fixed << setprecision(6);
  out << "# Iteration " << iter << " fitness=" << fitness << "\n"; // 标记行
  for (int t = 0; t < 7; ++t) {
    for (int k = 0; k < 4; ++k) {
      out << genes[t * 4 + k];
      if (k < 3) out << " ";
    }
    out << "\n";
  }
  out << "\n"; // 空行分隔
  return;
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

Individual randomIndividual(mt19937& gen) {
  Individual ind;
  uniform_real_distribution<double> dist(-5.0, 5.0);
  for (auto& ai : ind.ais) {
    vector<double> g(ai->geneCount());
    for (auto& v : g) v = dist(gen);
    ai->setGenes(g);
  }
  return ind;
}

// 前置carryOutOneGame的声明
void carryOutOneGame(const array<unique_ptr<BlockAI>, 7>& ais, mt19937& gen, int maxPieces);

double evalIndividual(const Individual& ind, mt19937& gen, int games = 3) {
  double total = 0; 
  for (int i = 0; i < games; ++i) {
    carryOutOneGame(ind.ais, gen, 200);
    total += g_lines;
  }
  return total / games; 
}

Individual tournamentSelect(const vector<Individual>& pop, mt19937& gen) {
  uniform_int_distribution<int> dist(0, (int)pop.size() - 1);
  int a = dist(gen), b = dist(gen);
  const Individual& winner = (pop[a].fitness > pop[b].fitness) ? pop[a] : pop[b];
  return winner.deepClone();
}

void crossover(Individual& p1, Individual& p2, mt19937& gen) {
  auto g1 = p1.allGenes();
  auto g2 = p2.allGenes();
  uniform_real_distribution<double> dist(0.0, 1.0);
  double alpha = dist(gen);
  for (size_t i = 0; i < g1.size(); ++i) {
    double v1 = g1[i], v2 = g2[i];
    g1[i] = alpha * v1 + (1 - alpha) * v2;
    g2[i] = alpha * v2 + (1 - alpha) * v1;
  }
  p1.setAllGenes(g1);
  p2.setAllGenes(g2);
}

void mutate(Individual& ind, mt19937& gen, double rate = 0.2, double sigma = 1.0) {
  auto g = ind.allGenes();
  uniform_real_distribution<double> prob(0.0, 1.0);
  normal_distribution<double> gauss(0.0, sigma);
  for (auto& v : g) if (prob(gen) < rate) v += gauss(gen);
  ind.setAllGenes(g);
}

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
	// 判断渲染是否开启
	if (!g_render) return;

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
	if (!g_render) return;

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
	if (!g_render) return;
	
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
  if (g_render) drawBlocks();
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
  cout << "最终等级: " << g_temporaryLevel << endl;// 解析数据行
  cout << "消去行数: " << g_lines << endl;
  return;
}

void initParameters() {
  // 初始化全局变量
  g_dice = -1, g_nextDice = 0; // 随机获取一个方块，同时记录下一个方块
  g_rotateNum = 0; // 设置旋转参数
  g_level = 1, g_temporaryLevel = 1; // 设置初始等级
  g_score = 0, g_lines = 0; // 记录分数、消去的行数
  g_quit = false; // 全局退出标志
  buffer = {}, temporaryBuffer = {}; // 清空棋盘上的所有信息，使用array不改变原有的大小
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = 0;
  }
  return;
}

/* 落点枚举 */
// 计算分数
Placements compareScore(const array<unique_ptr<BlockAI>, 7>& ais, Placements bestScore, int left, int ceil, int type, const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, int linesCleared) {
  // 复制一份（不允许窄化转换）
  Placements nowScore{
    static_cast<uint8_t>(left),
    static_cast<uint8_t>(ceil),
    static_cast<uint8_t>(type),
    calcFeatures(nextBuffer, linesCleared)
  };

  double s = ais[g_dice]->evaluate(nowScore.features);
  if (bestScore.left == numeric_limits<uint8_t>::max()) bestScore = nowScore;
  else {
    double best = ais[g_dice]->evaluate(bestScore.features);
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

bool simulateIsTouchBlockBelow(int ceil, int left) {
  bool ok = false;
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 4; ++i) {
      if (temporaryBlock[i][j] != COLOR_EMPTY && buffer[ceil + i + 1][left + j] != COLOR_EMPTY) {
        ok = true;
        break;
      }
    }
  }
  return ok;      
}

DropResult simulateNextBuffer(array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, int left) {
  nextBuffer = buffer;
  int ceil = g_nowCeil;
  int linesCleared = 0;
  while (true) {
    ceil++;
    if (simulateIsTouchBottom(ceil) || simulateIsTouchBlockBelow(ceil, left)) break;
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

// 向temporaryBlock压入执行每次旋转时压入旋转后的图形
void pushTemporaryBlock(int idx) {
  switch (g_dice) {
    case 0: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateI[idx][i][j];
      }
      break;
    }
    case 1: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateO[idx][i][j];
      }
      break;
    }
    case 2: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateT[idx][i][j];
      }
      break;
    }
    case 3: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateS[idx][i][j];
      }
      break;
    }
    case 4: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateZ[idx][i][j];
      }
      break;
    }
    case 5: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateJ[idx][i][j];
      }
      break;
    }
    case 6: {
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = rotateL[idx][i][j];
      }
      break;
    }
  }

  return;
}

// 更新尝试后最佳的位置
void bestPlace(Placements bestScore) {
  // 更新此时的全局变量g_nowCeil和g_nowLeft，方便统一打印图形
  g_nowCeil = bestScore.ceil;
  g_nowLeft = bestScore.left;
  return;
}

// 尝试所有的旋转形状
void tryAllRotateShapes(const array<unique_ptr<BlockAI>, 7>& ais) {
  Placements bestScore;
  // 初始化bestScore中的left为最大值
  bestScore.left = numeric_limits<uint8_t>::max();

  int rotateLen = 0;

  int left = 0, right = 0; // 定义左右边界

  // 定义ceil即记录得到的最低顶部值，定义linesCleared即记录得到的消行数
  int ceil = 0, linesCleared = 0;

  array<array<char, WIDTH + 5>, 2 * HIGHT + 5> nextBuffer = {};
  switch (g_dice) {
    case 0: {
      rotateLen = 2;
      for (int type = 0; type < rotateLen; type++) {
        pushTemporaryBlock(type);
        if (type == 0) left = 1, right = WIDTH - 2;
        else left = 0, right = WIDTH;
        for (; left < right; ++left) {
          auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, left);
          bestScore = compareScore(ais, bestScore, left, ceil, type, nextBuffer, linesCleared);
        }
      }
      pushTemporaryBlock(bestScore.rotate);
      break;
    }
    case 1: {
      pushTemporaryBlock(0);
      for (int left = 1; left < WIDTH; ++left) {
        auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, left);
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
          auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, left);
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
          auto [ceil, linesCleared] = simulateNextBuffer(nextBuffer, left);
          bestScore = compareScore(ais, bestScore, left, ceil, type, nextBuffer, linesCleared);
        }
      }
      pushTemporaryBlock(bestScore.rotate);
      break;
    }
    default: break;
  }
  bestPlace(bestScore);
  drawBlocks();
  return;
}

void simulateBlocksFallDown(const array<unique_ptr<BlockAI>, 7>& ais) {
  /* 初始化一个方块 */
  initBlocks();

  /* 绘制每帧界面 */
 	clearScreen();
  drawFrame();
  DropTimer timer(getFallInterval());

  /* 检测落点的同时选择分数最好的那个位置 */
  tryAllRotateShapes(ais);

  // 节约时间，直接采用硬降
  hardDrop(); 
  buffer = temporaryBuffer;
  return;
}

void carryOutOneGame(const array<unique_ptr<BlockAI>, 7>& ais, mt19937& gen, int maxPieces = 300) {
  initParameters();
  
  /* 初始化界面 */
  initArray();
  drawFrame();

  int pieces = 0;

  /* 绘制帧界面 */
  while (!isGameOver() && !g_quit && pieces < maxPieces) {
    simulateBlocksFallDown(ais);
    clearRow();
    pieces++;
    drawBlocks(); // 消行之后立即刷新（重绘分数）
    this_thread::sleep_for(chrono::milliseconds(200));
  }

  // 结束动画
  // drawEnd();

  // 延时显示
  // this_thread::sleep_for(chrono::milliseconds(100));
  return;
}

void evolution(vector<Individual>& pop, mt19937& gen) {
  ofstream log(log_path);
  log << fixed << setprecision(4);
  log << "Gen,BestFitness";
  for (int t = 0; t < 7; ++t) {
    for (int k = 0; k < 4; ++k) {
      log << ",T" << t << "_w" << k;
    }
  }
  log << "\n";
  
  for (int g = 0; g < GENS; ++g) {
    sort(pop.begin(), pop.end(), 
      [](const Individual& a, const Individual& b) { return a.fitness > b.fitness; });
    auto genes = pop[0].allGenes();
    
    cout << "Gen " << g << "best=" << pop[0].fitness << "\n";
    log << g << "," << pop[0].fitness;
    for (double v : genes) log << "," << v;
    log << "\n";
    
    vector<Individual> next;
    next.reserve(POP);
    for (int i = 0; i < ELITE; ++i) next.push_back(pop[i].deepClone());

    while ((int)next.size() < POP) {
      Individual c1 = tournamentSelect(pop, gen);
      Individual c2 = tournamentSelect(pop, gen);
      crossover(c1, c2, gen);
      mutate(c1, gen, MUT_RATE);
      mutate(c2, gen, MUT_RATE);
      c1.fitness = evalIndividual(c1, gen, 3);
      c2.fitness = evalIndividual(c2, gen, 3);
      next.push_back(move(c1));
      if ((int)next.size() < POP) next.push_back(move(c2));
    }
    pop = move(next);
  }
  
  // 最终结果
  sort(pop.begin(), pop.end(), 
    [](const Individual& a, const Individual& b) { return a.fitness > b.fitness; });

  log << "\n[Final Best]\n";
  log << "Fitness: " << pop[0].fitness << "\n";
  auto bestGenes = pop[0].allGenes();
  const char* names[7] = { "I", "O", "T", "S", "Z", "J", "L" };
  int idx = 0;
  for (int t = 0; t < 7; ++t) {
    log << names[t] << ": ";
    for (int k = 0; k < 4; ++k) log << bestGenes[idx++] << " "; 
    log << "\n";
  }
  log.close();

  // 写入最佳结果
  bestGenes = pop[0].allGenes();
  appendGenes(bestGenes, pop[0].fitness);

  cout << "\nFinal best fitness: " << pop[0].fitness << "\n";
  cout << "Log saved to log.txt\n";
  return;
}

void simulateGame() {
  // 定义随机数
  mt19937 gen(random_device{}());

  // 初始化种群
  vector<Individual> pop;
  pop.reserve(POP);
  
  // 尝试读入数据
  vector<double> genes;
	double historicalBest = 0.0;
  bool loaded = loadBestGroup(genes, historicalBest);

  if (loaded) {
    cout << "已加载历史最佳基因\n";

    // 载入上一组最优基因
    for (int i = 0; i < POP; ++i) {
      Individual ind;
      ind.setAllGenes(genes);
      if (i > 0) mutate(ind, gen, 0.3, 0.4);
      pop.push_back(move(ind));
      pop.back().fitness = evalIndividual(pop.back(), gen, 3);
      cout << "Init " << i << " fitness=" << pop.back().fitness << "\n";
    }

    // 分配手工权重个体
    // for (int i = POP / 2; i < POP / 2 + 5; ++i) {
      // Individual ind;
      // ind.setAllGenes(seedGenes);
      // if (i > 0) mutate(ind, gen, 0.2, 0.5);
      // pop.push_back(move(ind));
      // pop.back().fitness = evalIndividual(pop.back(), gen, 3);
      // cout << "Seed " << i << " fitness=" << pop.back().fitness << "\n";
    // }

    // 随机分配若干项
    // for (int i = POP / 2 + 5; i < POP; ++i) {
      // pop.push_back(randomIndividual(gen));
      // pop.back().fitness = evalIndividual(pop.back(), gen, 3);
      // cout << "Init " << i << " fitness=" << pop.back().fitness << "\n";
    // }
  } else {
    cout << "未找到历史数据，全部随机初始化\n";
    for (int i = 0; i < POP; ++i) {
      pop.push_back(randomIndividual(gen));
      pop.back().fitness = evalIndividual(pop.back(), gen, 3);
      cout << "Init " << i << " fitness=" << pop.back().fitness << "\n";
    }
  }

  // 进化
  evolution(pop, gen);
}

void autoPlay() {
  mt19937 gen(random_device{}());

	// 读入数据
	vector<double> genes;
	double historicalBest = 0.0;
	bool loaded = loadBestGroup(genes, historicalBest);
	
	// 载入基因
	Individual ind;
	ind.setAllGenes(genes);

  carryOutOneGame(ind.ais, gen, INT_MAX);

  drawEnd();
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

  
	/* 模拟游戏及进化（当g_train为true时） */
  if (g_train) simulateGame();
	// 否则进入自动游玩模式
	else autoPlay();

  // 恢复光标
  cout << "\033[?25h";
  // cout << "\033[?1049l";

  cout << "press Enter key to exit..." << endl;
  cin.get();
  return 0;
}

