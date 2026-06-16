#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// Задание 13. Случайный граф G(n,p) — оценки характеристик (вариант 3)
// MST, циклы, компоненты, клики; выборочное среднее и дисперсия (Уолфорд)
// Запуск: ./task_13 n trials [p]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// A000055: число неизоморфных деревьев на n вершинах with n nodes (n >= 1).
std::uint64_t unlabeledTreeCount(int n) {
    static const std::vector<std::uint64_t> table = {
        1ULL,   // n=1
        1ULL,   // 2
        1ULL,   // 3
        2ULL,   // 4
        3ULL,   // 5
        6ULL,   // 6
        11ULL,  // 7
        23ULL,  // 8
        47ULL,  // 9
        106ULL,  // 10
        235ULL,  // 11
        551ULL,  // 12
        1301ULL,  // 13
        3159ULL,  // 14
        7741ULL,  // 15
        19320ULL,  // 16
        48629ULL,  // 17
        123867ULL,  // 18
        317955ULL,  // 19
        823065ULL,  // 20
        2144505ULL,  // 21
        5623753ULL,  // 22
        14828074ULL,  // 23
        39299897ULL,  // 24
        104636890ULL  // 25
    };
    if (n < 1 || n > static_cast<int>(table.size())) {
        return 0;
    }
    return table[static_cast<std::size_t>(n - 1)];
}

// Система непересекающихся множеств (для Краскала)
struct UnionFind {
    std::vector<int> parent;
    std::vector<int> rankv;
    explicit UnionFind(int n) : parent(n), rankv(n, 0) {
        std::iota(parent.begin(), parent.end(), 0);
    }
    int find(int x) {
        if (parent[static_cast<std::size_t>(x)] == x) {
            return x;
        }
        return parent[static_cast<std::size_t>(x)] =
                   find(parent[static_cast<std::size_t>(x)]);
    }
    bool unite(int a, int b) {
        a = find(a);
        b = find(b);
        if (a == b) {
            return false;
        }
        if (rankv[static_cast<std::size_t>(a)] < rankv[static_cast<std::size_t>(b)]) {
            std::swap(a, b);
        }
        parent[static_cast<std::size_t>(b)] = a;
        if (rankv[static_cast<std::size_t>(a)] == rankv[static_cast<std::size_t>(b)]) {
            ++rankv[static_cast<std::size_t>(a)];
        }
        return true;
    }
};

struct Edge {
    int u = 0;
    int v = 0;
    int w = 0;
};

bool edgeLess(const Edge& a, const Edge& b) {
    return a.w < b.w;
}

// Представление случайного графа: список смежности и рёбра
struct RandomGraph {
    int n = 0;
    std::vector<std::vector<std::pair<int, int>>> adj;
    std::vector<Edge> edges;
    std::vector<int> degree;
};

// Генерация случайного графа G(n,p)
RandomGraph generateGraph(int n, double edgeProb, std::mt19937_64& rng) {
    RandomGraph g;
    g.n = n;
    g.adj.assign(n, {});
    g.degree.assign(n, 0);
    std::uniform_real_distribution<double> uni01(0.0, 1.0);
    std::uniform_int_distribution<int> weight(1, 10);

    for (int u = 0; u < n; ++u) {
        for (int v = u + 1; v < n; ++v) {
            if (uni01(rng) < edgeProb) {
                const int w = weight(rng);
                g.adj[static_cast<std::size_t>(u)].push_back({v, w});
                g.adj[static_cast<std::size_t>(v)].push_back({u, w});
                g.edges.push_back({u, v, w});
                ++g.degree[static_cast<std::size_t>(u)];
                ++g.degree[static_cast<std::size_t>(v)];
            }
        }
    }
    return g;
}

bool isConnected(const RandomGraph& g) {
    if (g.n == 0) {
        return true;
    }
    std::vector<char> vis(static_cast<std::size_t>(g.n), 0);
    std::vector<int> st;
    st.push_back(0);
    vis[0] = 1;
    int seen = 0;
    while (!st.empty()) {
        const int v = st.back();
        st.pop_back();
        ++seen;
        for (const auto& pr : g.adj[static_cast<std::size_t>(v)]) {
            const int u = pr.first;
            if (!vis[static_cast<std::size_t>(u)]) {
                vis[static_cast<std::size_t>(u)] = 1;
                st.push_back(u);
            }
        }
    }
    return seen == g.n;
}

// Вес минимального остовного дерева (алгоритм Краскала)
long long mstTotalWeight(const RandomGraph& g) {
    if (g.edges.empty()) {
        return 0;
    }
    std::vector<Edge> es = g.edges;
    std::sort(es.begin(), es.end(), edgeLess);
    UnionFind uf(g.n);
    long long sum = 0;
    int used = 0;
    for (const auto& e : es) {
        if (uf.unite(e.u, e.v)) {
            sum += e.w;
            ++used;
            if (used == g.n - 1) {
                break;
            }
        }
    }
    return sum;
}

int countIsolated(const RandomGraph& g) {
    int c = 0;
    for (int i = 0; i < g.n; ++i) {
        if (g.degree[static_cast<std::size_t>(i)] == 0) {
            ++c;
        }
    }
    return c;
}

std::vector<int> components(const RandomGraph& g) {
    std::vector<int> comp(static_cast<std::size_t>(g.n), -1);
    int cid = 0;
    for (int s = 0; s < g.n; ++s) {
        if (comp[static_cast<std::size_t>(s)] != -1) {
            continue;
        }
        std::vector<int> st{s};
        comp[static_cast<std::size_t>(s)] = cid;
        while (!st.empty()) {
            const int v = st.back();
            st.pop_back();
            for (const auto& pr : g.adj[static_cast<std::size_t>(v)]) {
                const int u = pr.first;
                if (comp[static_cast<std::size_t>(u)] == -1) {
                    comp[static_cast<std::size_t>(u)] = cid;
                    st.push_back(u);
                }
            }
        }
        ++cid;
    }
    return comp;
}

int countComponents(const RandomGraph& g) {
    if (g.n == 0) {
        return 0;
    }
    const auto comp = components(g);
    int mx = 0;
    for (int v : comp) {
        mx = std::max(mx, v);
    }
    return mx + 1;
}

bool isClique(const RandomGraph& g, const std::vector<int>& verts) {
    const int m = static_cast<int>(verts.size());
    if (m <= 1) {
        return true;
    }
    for (int i = 0; i < m; ++i) {
        for (int j = i + 1; j < m; ++j) {
            const int a = verts[static_cast<std::size_t>(i)];
            const int b = verts[static_cast<std::size_t>(j)];
            bool found = false;
            for (const auto& pr : g.adj[static_cast<std::size_t>(a)]) {
                if (pr.first == b) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
    }
    return true;
}

int countCliqueComponents(const RandomGraph& g) {
    if (g.n == 0) {
        return 0;
    }
    const auto comp = components(g);
    const int ccount = *std::max_element(comp.begin(), comp.end()) + 1;
    std::vector<std::vector<int>> verts(static_cast<std::size_t>(ccount));
    for (int v = 0; v < g.n; ++v) {
        verts[static_cast<std::size_t>(comp[static_cast<std::size_t>(v)])].push_back(v);
    }
    int ans = 0;
    for (const auto& vs : verts) {
        if (!vs.empty() && isClique(g, vs)) {
            ++ans;
        }
    }
    return ans;
}

// Enumerate simple cycles whose smallest vertex is `s` (only vertices > s or s on path).
// Updates longest cycle length (edges) and best cycle by max weight (ties: max edges).
void enumerateCyclesFromS(const RandomGraph& g, int s, int& bestCycleEdges,
                          long long& bestCycleWeight, int& bestEdgesAmongMaxWeight) {
    std::vector<int> path;
    path.reserve(static_cast<std::size_t>(g.n + 1));
    std::vector<char> onPath(static_cast<std::size_t>(g.n), 0);
    onPath[static_cast<std::size_t>(s)] = 1;

    const auto tryClose = [&](long long wOnPath, int wClose) {
        if (static_cast<int>(path.size()) < 3) {
            return;
        }
        const int edges = static_cast<int>(path.size());
        const long long totalW = wOnPath + wClose;
        bestCycleEdges = std::max(bestCycleEdges, edges);
        if (totalW > bestCycleWeight) {
            bestCycleWeight = totalW;
            bestEdgesAmongMaxWeight = edges;
        } else if (totalW == bestCycleWeight) {
            bestEdgesAmongMaxWeight = std::max(bestEdgesAmongMaxWeight, edges);
        }
    };

    std::function<void(int, long long)> dfs;
    dfs = [&](int at, long long wSum) {
        for (const auto& pr : g.adj[static_cast<std::size_t>(at)]) {
            const int to = pr.first;
            const int wt = pr.second;
            if (to == s) {
                tryClose(wSum, wt);
            } else if (to > s && !onPath[static_cast<std::size_t>(to)]) {
                path.push_back(to);
                onPath[static_cast<std::size_t>(to)] = 1;
                dfs(to, wSum + wt);
                onPath[static_cast<std::size_t>(to)] = 0;
                path.pop_back();
            }
        }
    };

    for (const auto& pr : g.adj[static_cast<std::size_t>(s)]) {
        const int to = pr.first;
        const int wt = pr.second;
        if (to <= s) {
            continue;
        }
        path.clear();
        path.push_back(s);
        path.push_back(to);
        onPath[static_cast<std::size_t>(to)] = 1;
        dfs(to, wt);
        onPath[static_cast<std::size_t>(to)] = 0;
        path.pop_back();
    }

    onPath[static_cast<std::size_t>(s)] = 0;
}

void analyzeCycles(const RandomGraph& g, int& longestCycleEdges, int& edgesInMaxWeightCycle) {
    longestCycleEdges = 0;
    long long bestWeight = -1;
    edgesInMaxWeightCycle = 0;
    for (int s = 0; s < g.n; ++s) {
        enumerateCyclesFromS(g, s, longestCycleEdges, bestWeight, edgesInMaxWeightCycle);
    }
    if (bestWeight < 0) {
        edgesInMaxWeightCycle = 0;
    }
}

// Накопление среднего и дисперсии (алгоритм Уолфорда)
struct Welford {
    std::uint64_t count = 0;
    long double mean = 0.0L;
    long double m2 = 0.0L;

    void add(long double x) {
        ++count;
        const long double d = x - mean;
        mean += d / static_cast<long double>(count);
        const long double d2 = x - mean;
        m2 += d * d2;
    }

    long double variance() const {
        if (count < 2) {
            return 0.0L;
        }
        return m2 / static_cast<long double>(count - 1);
    }
};

struct InputData {
    int n = 6;
    std::uint64_t trials = 5000;
    double edgeProb = 0.35;
};

bool parseIntPos(const char* text, int& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const int p = std::stoi(s, &pos);
        if (pos != s.size() || p <= 0) {
            return false;
        }
        value = p;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseU64(const char* text, std::uint64_t& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const auto v = std::stoull(s, &pos);
        if (pos != s.size() || v == 0ULL) {
            return false;
        }
        value = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseDouble01(const char* text, double& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const double v = std::stod(s, &pos);
        if (pos != s.size()) {
            return false;
        }
        if (v < 0.0 || v > 1.0) {
            return false;
        }
        value = v;
        return true;
    } catch (...) {
        return false;
    }
}

void printUsage(const char* exe) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exe << " n trials [p]\n\n";
    std::cerr << "  n       - number of vertices (recommended <= 10 for cycle enumeration)\n";
    std::cerr << "  trials  - Monte Carlo random graphs\n";
    std::cerr << "  p       - edge probability in G(n,p), default 0.35\n";
}

bool parseArgs(int argc, char* argv[], InputData& in) {
    if (argc < 3 || argc > 4) {
        return false;
    }
    if (!parseIntPos(argv[1], in.n) || !parseU64(argv[2], in.trials)) {
        return false;
    }
    if (argc == 4 && !parseDouble01(argv[3], in.edgeProb)) {
        return false;
    }
    return true;
}

}  // конец анонимного пространства имён

// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    InputData in;
    if (!parseArgs(argc, argv, in)) {
        printUsage(argv[0]);
        return 1;
    }

    std::mt19937_64 rng(std::random_device{}());

    Welford wA, wB, wD, wE, wG, wH;
    std::uint64_t connectedCount = 0;

    const std::uint64_t fValue = unlabeledTreeCount(in.n);
    const long double fConst = static_cast<long double>(fValue);

    const auto t0 = std::chrono::steady_clock::now();

    // Серия из trials реализаций G(n,p); накопление статистик Уолфорда
    for (std::uint64_t t = 0; t < in.trials; ++t) {
        const RandomGraph g = generateGraph(in.n, in.edgeProb, rng);

        const bool conn = isConnected(g);
        if (conn) {
            ++connectedCount;
            wA.add(static_cast<long double>(mstTotalWeight(g)));
        }

        int longest = 0;
        int edgesMaxW = 0;
        analyzeCycles(g, longest, edgesMaxW);
        wB.add(static_cast<long double>(longest));
        wD.add(static_cast<long double>(edgesMaxW));

        wE.add(static_cast<long double>(countIsolated(g)));
        wG.add(static_cast<long double>(countComponents(g)));
        wH.add(static_cast<long double>(countCliqueComponents(g)));
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double sec =
        std::chrono::duration<double>(t1 - t0).count();

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Model: random simple undirected graph G(n,p), n=" << in.n
              << ", p=" << in.edgeProb << ", edge weights uniform in {1..10}.\n";
    std::cout << "Monte Carlo trials: " << in.trials << "\n";
    std::cout << "Connected graphs (used for A): " << connectedCount << " / " << in.trials
              << "\n";
    std::cout << "Elapsed: " << sec << " s\n\n";

    const auto printStat = [](const char* name, const Welford& w, std::uint64_t used) {
        std::cout << name << " (samples=" << used << ")\n";
        if (used == 0) {
            std::cout << "  (no samples)\n\n";
            return;
        }
        std::cout << "  E[X] ~ " << w.mean << "\n";
        std::cout << "  Var[X] ~ " << w.variance() << "\n\n";
    };

    printStat("A: MST total weight (connected graphs only)", wA, connectedCount);
    printStat("B: longest simple cycle (edge count)", wB, in.trials);
    printStat("D: edges in a maximum-weight simple cycle (tie: max edges)", wD, in.trials);
    printStat("E: isolated vertices", wE, in.trials);
    printStat("G: connected components", wG, in.trials);
    printStat("H: connected components that are cliques", wH, in.trials);

    std::cout << "F: number of unlabeled trees on n vertices (A000055), deterministic\n";
    if (fValue == 0) {
        std::cout << "  (table only for 1 <= n <= 25; extend table for larger n)\n\n";
    } else {
        std::cout << "  F(" << in.n << ") = " << fValue << "\n";
        std::cout << "  E[F] = " << fConst << ", Var[F] = 0\n\n";
    }

    if (in.n > 10) {
        std::cout << "Note: cycle enumeration can be very slow for dense graphs with n > 10.\n";
    }

    return 0;
}
