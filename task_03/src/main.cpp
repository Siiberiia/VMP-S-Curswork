#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// Задание 3. Завод: сборка, переработка, качество (вариант 3)
// Дискретная имитационная модель производства; выход: times_L3/L4/L5.txt
// Запуск: ./task_03 AM RM tA tR n q1..qn s1..sn [modA modR]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// Пять уровней качества изделий и компонентов
constexpr int kQualityLevels = 5;
// Цель симуляции: 25 изделий максимального (5-го) уровня
constexpr int kLegendaryGoal = 25;

// Тип автомата: сборка или переработка
enum class MachineKind { Assembly, Recycle };

// Структура автомата на линии (сборка или переработка)
struct Machine {
    MachineKind kind = MachineKind::Assembly;
    int modules = 0;
    bool busy = false;
    int endTime = 0;

    // Текущая операция сборки (consumed components list)
    std::vector<std::pair<int, int>> assemblyConsumed;

    // Текущая операция переработки (product level)
    int recycleProductLevel = -1;
};

// Время цикла с учётом числа модулей качества
int cycleTimeTicks(int baseTicks, int modules) {
    const double factor = 1.0 + 0.1 * static_cast<double>(modules);
    int t = static_cast<int>(std::llround(static_cast<double>(baseTicks) * factor));
    if (t < 1) {
        t = 1;
    }
    return t;
}

// Для m модулей: P(скачок k) = m·52·10^(-2-k); остаток — «без изменения» (k=0)
std::vector<double> // Распределение скачка уровня качества при переработке/сборке
std::vector<double> buildUpgradeMasses(int modules) {
    std::vector<double> masses;
    double sum = 0.0;
    for (int k = 1; k <= 40; ++k) {
        const double p = static_cast<double>(modules) * 52.0 * std::pow(10.0, -2.0 - static_cast<double>(k));
        if (p <= 1e-18) {
            break;
        }
        masses.push_back(p);
        sum += p;
    }
    if (sum > 1.0) {
        for (double& x : masses) {
            x /= sum;
        }
        sum = 1.0;
    }
    const double stay = std::max(0.0, 1.0 - sum);
    masses.insert(masses.begin(), stay);
    return masses;
}

int sampleQualityDelta(std::mt19937_64& rng, const std::vector<double>& masses) {
    std::discrete_distribution<int> dist(masses.begin(), masses.end());
    return dist(rng);  // 0 — без изменения качества, иначе скачок на k
}

bool consumeRecipeGreedy(std::vector<std::vector<long long>>& inv, const std::vector<int>& q,
                         std::vector<std::pair<int, int>>& consumedOut) {
    consumedOut.clear();
    const int n = static_cast<int>(q.size());
    for (int comp = 0; comp < n; ++comp) {
        int need = q[static_cast<std::size_t>(comp)];
        for (int lvl = 1; lvl <= kQualityLevels && need > 0; ++lvl) {
            long long& have = inv[static_cast<std::size_t>(comp)][static_cast<std::size_t>(lvl)];
            if (have <= 0) {
                continue;
            }
            const long long take = std::min<long long>(have, need);
            have -= take;
            need -= static_cast<int>(take);
            for (long long u = 0; u < take; ++u) {
                consumedOut.push_back({comp, lvl});
            }
        }
        if (need > 0) {
            for (const auto& pr : consumedOut) {
                inv[static_cast<std::size_t>(pr.first)][static_cast<std::size_t>(pr.second)] += 1;
            }
            consumedOut.clear();
            return false;
        }
    }
    return true;
}

int minConsumedLevel(const std::vector<std::pair<int, int>>& consumed) {
    int m = kQualityLevels + 1;
    for (const auto& pr : consumed) {
        m = std::min(m, pr.second);
    }
    return m;
}

int recycleOutputPieces(int q) {
    // ceil(q/4) — дискретное приближение 25% рецепта при переработке
    return (q + 3) / 4;
}

void printUsage(const char* exe) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exe
              << " AM RM t_assembly t_recycle n q1..qn s1..sn [assembly_modules recycle_modules]\n\n";
    std::cerr << "Where:\n";
    std::cerr << "  AM, RM - counts of assembly / recycling machines (>=0)\n";
    std::cerr << "  t_assembly, t_recycle - base cycle times in discrete time units (>=1)\n";
    std::cerr << "  n - number of recipe components (>=1)\n";
    std::cerr << "  q1..qn - integer quantities per product\n";
    std::cerr << "  s1..sn - inbound supply rates of level-1 components per discrete time unit\n";
    std::cerr << "  assembly_modules, recycle_modules - optional, in [0..4], default 0\n\n";
    std::cerr << "Outputs:\n";
    std::cerr << "  times_L3.txt, times_L4.txt, times_L5.txt - assembly production timestamps\n";
}

bool parseInt(const char* s, int& v) {
    try {
        std::string t(s);
        std::size_t pos = 0;
        const long long x = std::stoll(t, &pos);
        if (pos != t.size()) {
            return false;
        }
        if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max()) {
            return false;
        }
        v = static_cast<int>(x);
        return true;
    } catch (...) {
        return false;
    }
}

}  // конец анонимного пространства имён

// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    if (argc < 7) {
        printUsage(argv[0]);
        return 1;
    }

    int AM = 0;
    int RM = 0;
    int tA = 0;
    int tR = 0;
    int n = 0;
    if (!parseInt(argv[1], AM) || !parseInt(argv[2], RM) || !parseInt(argv[3], tA) || !parseInt(argv[4], tR) ||
        !parseInt(argv[5], n)) {
        printUsage(argv[0]);
        return 1;
    }
    if (AM < 0 || RM < 0 || tA < 1 || tR < 1 || n < 1) {
        std::cerr << "Invalid AM/RM/times/n.\n";
        return 1;
    }

    const int expected = 6 + 2 * n;
    if (argc != expected && argc != expected + 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<int> q(static_cast<std::size_t>(n));
    std::vector<int> s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (!parseInt(argv[6 + i], q[static_cast<std::size_t>(i)])) {
            std::cerr << "Invalid q[" << i << "].\n";
            return 1;
        }
        if (q[static_cast<std::size_t>(i)] < 1) {
            std::cerr << "All q must be >= 1.\n";
            return 1;
        }
    }
    for (int i = 0; i < n; ++i) {
        if (!parseInt(argv[6 + n + i], s[static_cast<std::size_t>(i)])) {
            std::cerr << "Invalid s[" << i << "].\n";
            return 1;
        }
        if (s[static_cast<std::size_t>(i)] < 0) {
            std::cerr << "All s must be >= 0.\n";
            return 1;
        }
    }

    int modA = 0;
    int modR = 0;
    if (argc == expected + 2) {
        if (!parseInt(argv[expected], modA) || !parseInt(argv[expected + 1], modR)) {
            std::cerr << "Invalid module counts.\n";
            return 1;
        }
    }
    if (modA < 0 || modA > 4 || modR < 0 || modR > 4) {
        std::cerr << "Module counts must be in [0..4].\n";
        return 1;
    }

    std::vector<Machine> machines;
    machines.reserve(static_cast<std::size_t>(AM + RM));
    for (int i = 0; i < AM; ++i) {
        machines.push_back({MachineKind::Assembly, modA, false, 0, {}, -1});
    }
    for (int i = 0; i < RM; ++i) {
        machines.push_back({MachineKind::Recycle, modR, false, 0, {}, -1});
    }

    const int tickAssembly = cycleTimeTicks(tA, modA);
    const int tickRecycle = cycleTimeTicks(tR, modR);

    const std::vector<double> massAssembly = buildUpgradeMasses(modA);
    const std::vector<double> massRecycle = buildUpgradeMasses(modR);

    std::mt19937_64 rng(std::random_device{}());

    std::vector<std::vector<long long>> inv(static_cast<std::size_t>(n),
                                             std::vector<long long>(static_cast<std::size_t>(kQualityLevels + 1), 0));
    std::vector<long long> products(static_cast<std::size_t>(kQualityLevels + 1), 0);

    std::vector<int> timesL3;
    std::vector<int> timesL4;
    std::vector<int> timesL5;
    int legendaryCount = 0;

    const int maxTime = 50'000'000;
    int t = 0;
    // Главный цикл дискретного моделирования (тики времени до 25 изделий 5-го уровня)
    for (; t <= maxTime; ++t) {
        // Поставки компонентов на склад (уровень 1)
        for (int i = 0; i < n; ++i) {
            inv[static_cast<std::size_t>(i)][1] += s[static_cast<std::size_t>(i)];
        }

        // Завершение операций автоматов в момент t
        for (auto& m : machines) {
            if (!m.busy || m.endTime != t) {
                continue;
            }
            m.busy = false;

            if (m.kind == MachineKind::Assembly) {
                if (m.assemblyConsumed.empty()) {
                    throw std::runtime_error("Internal error: assembly job without consumed list.");
                }
                const int L = minConsumedLevel(m.assemblyConsumed);
                const int k = sampleQualityDelta(rng, massAssembly);
                int outLevel = L + k;
                outLevel = std::clamp(outLevel, 1, kQualityLevels);
                products[static_cast<std::size_t>(outLevel)] += 1;
                if (outLevel == 3) {
                    timesL3.push_back(t);
                } else if (outLevel == 4) {
                    timesL4.push_back(t);
                } else if (outLevel == 5) {
                    timesL5.push_back(t);
                    ++legendaryCount;
                }
                m.assemblyConsumed.clear();
                m.recycleProductLevel = -1;
            } else {  // переработка
                if (m.recycleProductLevel < 1) {
                    throw std::runtime_error("Internal error: recycle job without product level.");
                }
                const int L = m.recycleProductLevel;
                const int k = sampleQualityDelta(rng, massRecycle);
                int compLevel = L + k;
                compLevel = std::clamp(compLevel, 1, kQualityLevels);
                for (int i = 0; i < n; ++i) {
                    const int add = recycleOutputPieces(q[static_cast<std::size_t>(i)]);
                    inv[static_cast<std::size_t>(i)][static_cast<std::size_t>(compLevel)] += add;
                }
                m.assemblyConsumed.clear();
                m.recycleProductLevel = -1;
            }
        }

        if (legendaryCount >= kLegendaryGoal) {
            break;
        }

        // Запуск новых операций в момент t (после завершений)
        for (auto& m : machines) {
            if (m.busy) {
                continue;
            }
            if (m.kind == MachineKind::Assembly) {
                std::vector<std::pair<int, int>> consumed;
                if (!consumeRecipeGreedy(inv, q, consumed)) {
                    continue;
                }
                m.busy = true;
                m.endTime = t + tickAssembly;
                m.assemblyConsumed = std::move(consumed);
                m.recycleProductLevel = -1;
                continue;
            }

            // Переработка: берём изделие минимального доступного уровня
            int takeLevel = -1;
            for (int lvl = 1; lvl <= kQualityLevels; ++lvl) {
                if (products[static_cast<std::size_t>(lvl)] > 0) {
                    takeLevel = lvl;
                    break;
                }
            }
            if (takeLevel < 0) {
                continue;
            }
            products[static_cast<std::size_t>(takeLevel)] -= 1;
            m.busy = true;
            m.endTime = t + tickRecycle;
            m.assemblyConsumed.clear();
            m.recycleProductLevel = takeLevel;
        }

        if (legendaryCount >= kLegendaryGoal) {
            break;
        }
    }

    if (legendaryCount < kLegendaryGoal) {
        std::cerr << "Warning: did not reach " << kLegendaryGoal
                  << " legendary products within maxTime=" << maxTime << ".\n";
    }

    auto writeTimes = [](const std::string& path, const std::vector<int>& ts) {
        std::ofstream out(path);
        if (!out) {
            throw std::runtime_error("Cannot write: " + path);
        }
        for (int v : ts) {
            out << v << "\n";
        }
    };

    writeTimes("times_L3.txt", timesL3);
    writeTimes("times_L4.txt", timesL4);
    writeTimes("times_L5.txt", timesL5);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Simulation finished at t=" << t << "\n";
    std::cout << "Legendary products produced: " << legendaryCount << " / " << kLegendaryGoal << "\n";
    std::cout << "Counts (assembly outputs): L3=" << timesL3.size() << " L4=" << timesL4.size() << " L5=" << timesL5.size()
              << "\n";
    std::cout << "Wrote: times_L3.txt, times_L4.txt, times_L5.txt\n";

    return 0;
}
