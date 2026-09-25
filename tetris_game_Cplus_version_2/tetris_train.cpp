#include <iostream>
#include <string>
#include <array>
#include <random>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>

using namespace std;

// ---------------- 常量 ----------------
static int constexpr WIDTH = 10;
static int constexpr HIGHT = 20;

static int constexpr POP = 30;
static int constexpr GENS = 20;
static int constexpr ELITE = 3;
static double constexpr MUT_RATE = 0.2;

// ---------------- 全局变量 ----------------
uint8_t g_dice = -1, g_nextDice = 0;
uint8_t g_rotateNum = 0;
uint8_t g_nowCeil, g_nowLeft;
uint32_t g_level = 1, g_temporaryLevel = 1;
uint32_t g_score = 0, g_lines = 0;
bool g_quit = false;
array<array<char, WIDTH + 5>, 2 * HIGHT + 5> buffer = {}, temporaryBuffer = {};
int temporaryBlock[4][4] = {
  {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}
};

const string path = "train_gene_progress.txt";
const string log_path = "train_log.txt";

static const int baseScore[] = {0, 100, 300, 500, 800};

// ---------------- 随机数 ----------------
static mt19937& get_engine() {
  static mt19937 engine(std::random_device{}());
  return engine;
}

int get_random_int(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);
  return dist(get_engine());
}

// ---------------- 结构体 ----------------
struct Features {
  double linesCleared;
  double aggregateHeight;
  double holes;
  double bumpiness;
};

struct Placements {
  uint8_t left;
  uint8_t ceil;
  uint8_t rotate;
  Features features;
};

struct DropResult {
  int ceil;
  int linesCleared;
};

// ---------------- BlockAI 基类 + 7 派生类 ----------------
class BlockAI {
public:
  virtual ~BlockAI() = default;
  virtual double evaluate(const Features& f) const = 0;
  virtual vector<double> getGenes() const = 0;
  virtual void setGenes(const vector<double>& genes) = 0;
  virtual int geneCount() const = 0;
  virtual unique_ptr<BlockAI> clone() const = 0;
  virtual int type() const = 0;
};

#define DEFINE_BLOCK_AI(NAME, TYPE_ID)                       \
class NAME : public BlockAI {                                \
  array<double, 4> w_{};                                     \
public:                                                      \
  NAME() = default;                                          \
  explicit NAME(const array<double, 4>& w) : w_(w) {}        \
  double evaluate(const Features& f) const override {        \
    return w_[0] * f.linesCleared                            \
         + w_[1] * f.aggregateHeight                         \
         + w_[2] * f.holes                                   \
         + w_[3] * f.bumpiness;                              \
  }                                                          \
  vector<double> getGenes() const override {                 \
    return { w_[0], w_[1], w_[2], w_[3] };                   \
  }                                                          \
  void setGenes(const vector<double>& g) override {          \
    for (int i = 0; i < 4 && i < (int)g.size(); ++i) w_[i] = g[i]; \
  }                                                          \
  int geneCount() const override { return 4; }               \
  unique_ptr<BlockAI> clone() const override {               \
    return make_unique<NAME>(*this);                         \
  }                                                          \
  int type() const override { return TYPE_ID; }              \
};

DEFINE_BLOCK_AI(IBlockAI, 0)
DEFINE_BLOCK_AI(OBlockAI, 1)
DEFINE_BLOCK_AI(TBlockAI, 2)
DEFINE_BLOCK_AI(SBlockAI, 3)
DEFINE_BLOCK_AI(ZBlockAI, 4)
DEFINE_BLOCK_AI(JBlockAI, 5)
DEFINE_BLOCK_AI(LBlockAI, 6)

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

// ---------------- 方块形状 ----------------
const int SHAPES[7][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, // I
  {{2,2,0,0},{2,2,0,0},{0,0,0,0},{0,0,0,0}}, // O
  {{0,3,0,0},{3,3,3,0},{0,0,0,0},{0,0,0,0}}, // T
  {{0,4,4,0},{4,4,0,0},{0,0,0,0},{0,0,0,0}}, // S
  {{5,5,0,0},{0,5,5,0},{0,0,0,0},{0,0,0,0}}, // Z
  {{6,0,0,0},{6,6,6,0},{0,0,0,0},{0,0,0,0}}, // J
  {{0,0,7,0},{7,7,7,0},{0,0,0,0},{0,0,0,0}}  // L
};

const int rotateI[2][4][4] = {
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
  {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}
};
const int rotateO[1][4][4] = {
  {{2,2,0,0},{2,2,0,0},{0,0,0,0},{0,0,0,0}}
};
const int rotateT[4][4][4] = {
  {{0,3,0,0},{3,3,3,0},{0,0,0,0},{0,0,0,0}},
  {{0,3,0,0},{0,3,3,0},{0,3,0,0},{0,0,0,0}},
  {{0,0,0,0},{3,3,3,0},{0,3,0,0},{0,0,0,0}},
  {{0,3,0,0},{3,3,0,0},{0,3,0,0},{0,0,0,0}}
};
const int rotateS[2][4][4] = {
  {{0,4,4,0},{4,4,0,0},{0,0,0,0},{0,0,0,0}},
  {{0,4,0,0},{0,4,4,0},{0,0,4,0},{0,0,0,0}}
};
const int rotateZ[2][4][4] = {
  {{5,5,0,0},{0,5,5,0},{0,0,0,0},{0,0,0,0}},
  {{0,0,5,0},{0,5,5,0},{0,5,0,0},{0,0,0,0}}
};
const int rotateJ[4][4][4] = {
  {{6,0,0,0},{6,6,6,0},{0,0,0,0},{0,0,0,0}},
  {{0,6,6,0},{0,6,0,0},{0,6,0,0},{0,0,0,0}},
  {{0,0,0,0},{6,6,6,0},{0,0,6,0},{0,0,0,0}},
  {{0,6,0,0},{0,6,0,0},{6,6,0,0},{0,0,0,0}}
};
const int rotateL[4][4][4] = {
  {{0,0,7,0},{7,7,7,0},{0,0,0,0},{0,0,0,0}},
  {{0,7,0,0},{0,7,0,0},{0,7,7,0},{0,0,0,0}},
  {{0,0,0,0},{7,7,7,0},{7,0,0,0},{0,0,0,0}},
  {{7,7,0,0},{0,7,0,0},{0,7,0,0},{0,0,0,0}}
};

// ---------------- 文件读写 ----------------
int countValidLines() {
  ifstream in(path);
  if (!in) return -1;
  int lines = 0;
  string line;
  while (getline(in, line)) {
    if (line.empty()) continue;
    if (line[0] == '#' || line[0] == '[') continue;
    lines++;
  }
  return lines;
}

void appendGenes(const vector<double>& genes, double fitness) {
  int iter = countValidLines() / 7;
  ofstream out(path, ios::app);
  if (!out) return;
  out << fixed << setprecision(6);
  out << "# Iteration " << iter << " fitness=" << fitness << "\n";
  for (int t = 0; t < 7; ++t) {
    for (int k = 0; k < 4; ++k) {
      out << genes[t * 4 + k];
      if (k < 3) out << " ";
    }
    out << "\n";
  }
  out << "\n";
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

// ---------------- 特征计算 ----------------
Features calcFeatures(const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, int linesCleared) {
  array<int, WIDTH> heights{};
  for (int j = 1; j < WIDTH + 1; ++j) {
    for (int i = HIGHT; i < 2 * HIGHT; ++i) {
      if (nextBuffer[i][j] != 0) {
        heights[j - 1] = 2 * HIGHT - i;
        break;
      }
    }
  }
  double agg = 0;
  for (int h : heights) agg += h;
  double holes = 0;
  for (int j = 1; j < WIDTH + 1; ++j) {
    bool seen = false;
    for (int i = HIGHT; i < 2 * HIGHT; ++i) {
      if (nextBuffer[i][j] != 0) seen = true;
      else if (seen) holes++;
    }
  }
  double bump = 0;
  for (int j = 0; j + 1 < WIDTH; ++j) bump += abs(heights[j] - heights[j + 1]);
  return { (double)linesCleared, agg, holes, bump };
}

// ---------------- Individual ----------------
struct Individual {
  array<unique_ptr<BlockAI>, 7> ais;
  double fitness = 0;

  Individual() {
    for (int i = 0; i < 7; i++) ais[i] = makeAI(i);
  }

  Individual(const Individual&) = delete;
  Individual& operator=(const Individual&) = delete;
  Individual(Individual&&) = default;
  Individual& operator=(Individual&&) = default;

  Individual deepClone() const {
    Individual c;
    for (int i = 0; i < 7; ++i) c.ais[i] = ais[i]->clone();
    c.fitness = fitness;
    return c;
  }

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

// ---------------- 模拟核心 ----------------
void initParameters() {
  g_dice = -1, g_nextDice = 0;
  g_rotateNum = 0;
  g_nowCeil = 0, g_nowLeft = 0;
  g_level = 1, g_temporaryLevel = 1;
  g_score = 0, g_lines = 0;
  g_quit = false;
  buffer = {}, temporaryBuffer = {};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) temporaryBlock[i][j] = 0;
}

void initBlocks() {
  g_rotateNum = 0;
  if (g_dice == -1) g_dice = get_random_int(0, 6);
  else g_dice = g_nextDice;
  g_nextDice = get_random_int(0, 6);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      temporaryBlock[i][j] = SHAPES[g_dice][i][j];
  g_nowCeil = HIGHT - 2;
  g_nowLeft = WIDTH / 2;
}

int isRowFull(int row) {
  int cnt = 0;
  for (int j = 1; j <= WIDTH; j++)
    if (buffer[row][j] != 0) cnt++;
  return cnt;
}

void deleteRowInBuffer(int row) {
  for (; row > HIGHT; row--)
    for (int j = 1; j <= WIDTH; j++)
      buffer[row][j] = buffer[row - 1][j];
  for (int j = 1; j <= WIDTH; j++) buffer[row][j] = 0;
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
}

bool isGameOver() {
  for (int j = 1; j <= WIDTH; j++)
    if (buffer[HIGHT - 1][j] != 0) return true;
  return false;
}

// ---------------- 落点枚举 ----------------
Placements compareScore(const array<unique_ptr<BlockAI>, 7>& ais,
                        Placements bestScore, int left, int ceil, int type,
                        const array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer,
                        int linesCleared) {
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
    if (s > best) bestScore = nowScore;
  }
  return bestScore;
}

bool simulateIsTouchBottom(int ceil) {
  for (int j = 0; j < 4; ++j) {
    int floor = 0;
    for (int i = 0; i < 4; ++i)
      if (temporaryBlock[i][j] != 0) floor = i + 1;
    if (ceil + floor >= 2 * HIGHT) return true;
  }
  return false;
}

bool simulateIsTouchBlockBelow(int ceil, int left) {
  for (int j = 0; j < 4; ++j)
    for (int i = 0; i < 4; ++i)
      if (temporaryBlock[i][j] != 0 && buffer[ceil + i + 1][left + j] != 0)
        return true;
  return false;
}

DropResult simulateNextBuffer(array<array<char, WIDTH + 5>, 2 * HIGHT + 5>& nextBuffer, int left) {
  nextBuffer = buffer;
  int ceil = g_nowCeil;
  int linesCleared = 0;
  while (true) {
    ceil++;
    if (simulateIsTouchBottom(ceil) || simulateIsTouchBlockBelow(ceil, left)) break;
  }
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      if (nextBuffer[ceil + i][left + j] == 0)
        nextBuffer[ceil + i][left + j] = temporaryBlock[i][j];

  for (int i = 2 * HIGHT - 1; i >= HIGHT; --i) {
    bool full = true;
    for (int j = 1; j <= WIDTH; ++j) {
      if (nextBuffer[i][j] == 0) { full = false; break; }
    }
    if (full) {
      for (int k = i; k > HIGHT; --k) nextBuffer[k] = nextBuffer[k - 1];
      for (int j = 1; j <= WIDTH; ++j) nextBuffer[HIGHT][j] = 0;
      linesCleared++;
      i++;
    }
  }
  return {ceil, linesCleared};
}

void pushTemporaryBlock(int idx) {
  switch (g_dice) {
    case 0: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateI[idx][i][j]; break;
    case 1: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateO[idx][i][j]; break;
    case 2: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateT[idx][i][j]; break;
    case 3: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateS[idx][i][j]; break;
    case 4: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateZ[idx][i][j]; break;
    case 5: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateJ[idx][i][j]; break;
    case 6: for (int i=0;i<4;++i) for (int j=0;j<4;++j) temporaryBlock[i][j]=rotateL[idx][i][j]; break;
  }
}

void bestPlace(Placements bestScore) {
  g_nowCeil = bestScore.ceil;
  g_nowLeft = bestScore.left;
}

void tryAllRotateShapes(const array<unique_ptr<BlockAI>, 7>& ais) {
  Placements bestScore;
  bestScore.left = numeric_limits<uint8_t>::max();
  int rotateLen = 0;
  int left = 0, right = 0;
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
    case 5: case 6: case 2: {
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
    case 3: case 4: {
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
  }
  bestPlace(bestScore);
}

// ---------------- 纯计算版一局 ----------------
void simulateBlocksFallDown(const array<unique_ptr<BlockAI>, 7>& ais) {
  initBlocks();
  tryAllRotateShapes(ais);
  // 直接把方块固化进 buffer
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      if (temporaryBlock[i][j] != 0)
        buffer[g_nowCeil + i][g_nowLeft + j] = temporaryBlock[i][j];
}

int simulateOneGame(const array<unique_ptr<BlockAI>, 7>& ais, mt19937& gen, int maxPieces = 200) {
  initParameters();
  int pieces = 0;
  while (!isGameOver() && pieces < maxPieces) {
    clearRow();
    simulateBlocksFallDown(ais);
    pieces++;
  }
  return g_lines;
}

// ---------------- 遗传算法 ----------------
double evalIndividual(const Individual& ind, mt19937& gen, int games = 3) {
  double total = 0;
  for (int i = 0; i < games; ++i)
    total += simulateOneGame(ind.ais, gen, 200);
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

void evolution(vector<Individual>& pop, mt19937& gen) {
  ofstream log(log_path);
  log << fixed << setprecision(4);
  log << "Gen,BestFitness";
  for (int t = 0; t < 7; ++t)
    for (int k = 0; k < 4; ++k)
      log << ",T" << t << "_w" << k;
  log << "\n";

  for (int g = 0; g < GENS; ++g) {
    sort(pop.begin(), pop.end(),
      [](const Individual& a, const Individual& b) { return a.fitness > b.fitness; });

    auto genes = pop[0].allGenes();
    cout << "Gen " << g << " best=" << pop[0].fitness << "\n";
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

  appendGenes(bestGenes, pop[0].fitness);

  cout << "\nFinal best fitness: " << pop[0].fitness << "\n";
  cout << "Log saved to log.txt\n";
}

// ---------------- 入口 ----------------
void simulateGame() {
  mt19937 gen(random_device{}());

  vector<Individual> pop;
  pop.reserve(POP);

  vector<double> genes;
  double historicalBest = 0.0;
  bool loaded = loadBestGroup(genes, historicalBest);

  if (loaded) {
    cout << "已加载历史最佳基因\n";
    for (int i = 0; i < POP; ++i) {
      Individual ind;
      ind.setAllGenes(genes);
      if (i > 0) mutate(ind, gen, 0.5, 0.3);
      pop.push_back(move(ind));
      pop.back().fitness = evalIndividual(pop.back(), gen, 3);
      cout << "Init " << i << " fitness=" << pop.back().fitness << "\n";
    }
  } else {
    cout << "未找到历史数据，全部随机初始化\n";
    for (int i = 0; i < POP; ++i) {
      pop.push_back(randomIndividual(gen));
      pop.back().fitness = evalIndividual(pop.back(), gen, 3);
      cout << "Init " << i << " fitness=" << pop.back().fitness << "\n";
    }
  }

  evolution(pop, gen);
}

int main() {
  simulateGame();
  cout << "press Enter key to exit..." << endl;
  cin.get();
  return 0;
}
