#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

// =============================================================================
// Задание 10. Дети в семье — распределение Пуассона (вариант 3)
// K~Poisson(λ=1.94); сравнение P(K≥m) и P(K≥m | нет девочек)
// Запуск: ./task_10 m [trials]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// Параметр Пуассона из условия варианта
constexpr long double kLambda = 1.94L;

// P(X >= m) for X ~ Poisson(mu), integer m >= 1.
// Хвост распределения Пуассона: P(X ≥ m)
long double poissonTailGeM(long double mu, int m) {
    if (m <= 0) {
        return 1.0L;
    }
    long double term = std::exp(-mu);  // P(X = 0)
    long double cdfUpToMMinus1 = 0.0L;
    for (int k = 0; k < m; ++k) {
        cdfUpToMMinus1 += term;
        if (k < m - 1) {
            term *= mu / static_cast<long double>(k + 1);
        }
    }
    return 1.0L - cdfUpToMMinus1;
}

struct InputData {
    int m = 1;
    std::uint64_t trials = 500000;
};

bool parsePositiveInt(const char* text, int& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const int parsed = std::stoi(s, &pos);
        if (pos != s.size() || parsed <= 0) {
            return false;
        }
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseU64(const char* text, std::uint64_t& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const auto parsed = std::stoull(s, &pos);
        if (pos != s.size() || parsed == 0ULL) {
            return false;
        }
        value = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

void printUsage(const char* exeName) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exeName << " m [trials]\n\n";
    std::cerr << "  m       - threshold (integer m >= 1), event: number of children K >= m\n";
    std::cerr << "  trials  - Monte Carlo families (default: 500000)\n";
    std::cerr << "\nModel: K ~ Poisson(lambda), lambda = " << kLambda << ".\n";
    std::cerr << "Given K=k, each gender pattern is equally likely (fair coin per child).\n";
}

bool parseArgs(int argc, char* argv[], InputData& in) {
    if (argc < 2 || argc > 3) {
        return false;
    }
    if (!parsePositiveInt(argv[1], in.m)) {
        return false;
    }
    if (argc == 3 && !parseU64(argv[2], in.trials)) {
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

    const long double muCond = kLambda / 2.0L;
    const long double pCondAnalytical = poissonTailGeM(muCond, in.m);
    const long double pUncondAnalytical = poissonTailGeM(kLambda, in.m);

    std::mt19937_64 rng(std::random_device{}());
    std::poisson_distribution<int> distK(static_cast<double>(kLambda));
    std::bernoulli_distribution boy(0.5);

    std::uint64_t countNoGirls = 0;
    std::uint64_t countNoGirlsAndKgeM = 0;
    std::uint64_t countKgeM = 0;

    // Монте-Карло: генерируем семьи K~Poisson(λ), пол каждого ребёнка — бернулли 0.5
    for (std::uint64_t t = 0; t < in.trials; ++t) {
        const int k = distK(rng);
        bool allBoys = true;
        if (k > 0) {
            for (int i = 0; i < k; ++i) {
                if (!boy(rng)) {
                    allBoys = false;
                    break;
                }
            }
        }
        if (k >= in.m) {
            ++countKgeM;
        }
        if (allBoys) {
            ++countNoGirls;
            if (k >= in.m) {
                ++countNoGirlsAndKgeM;
            }
        }
    }

    const long double pUncondEmp =
        static_cast<long double>(countKgeM) / static_cast<long double>(in.trials);
    long double pCondEmp = 0.0L;
    if (countNoGirls > 0) {
        pCondEmp = static_cast<long double>(countNoGirlsAndKgeM) /
                   static_cast<long double>(countNoGirls);
    }

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "lambda = " << kLambda << ", m = " << in.m
              << ", trials = " << in.trials << "\n\n";

    std::cout << "Analytical:\n";
    std::cout << "  P(K >= m)                    = " << pUncondAnalytical << "\n";
    std::cout << "  P(K >= m | no girls)         = " << pCondAnalytical << "\n";
    std::cout << "  (equivalently: Y ~ Poisson(lambda/2), P(Y >= m))\n";
    std::cout << "  Difference (conditional - unconditional) = "
              << (pCondAnalytical - pUncondAnalytical) << "\n\n";

    std::cout << "Empirical:\n";
    std::cout << "  P_hat(K >= m)                = " << pUncondEmp << "\n";
    if (countNoGirls > 0) {
        std::cout << "  P_hat(K >= m | no girls)     = " << pCondEmp << "\n";
        std::cout << "  Families with no girls:      " << countNoGirls << "\n";
        std::cout << "  |P_hat_cond - P_analytical| = "
                  << std::fabs(pCondEmp - pCondAnalytical) << "\n";
    } else {
        std::cout << "  No 'no girls' families in sample; increase trials.\n";
    }
    std::cout << "  |P_hat_uncond - P_analytical| = "
              << std::fabs(pUncondEmp - pUncondAnalytical) << "\n";

    return 0;
}
