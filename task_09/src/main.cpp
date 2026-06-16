#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

// =============================================================================
// Задание 9. Монеты в кошельке — условная вероятность (вариант 3)
// k двуорловых из n; P(4-й орёл | первые 3 орла)
// Запуск: ./task_09 n k [trials]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

struct InputData {
    std::uint64_t n = 0;
    std::uint64_t k = 0;
    std::uint64_t trials = 1000000;
};

bool parseU64(const char* text, std::uint64_t& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const auto parsed = std::stoull(s, &pos);
        if (pos != s.size()) {
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
    std::cerr << "  " << exeName << " n k [trials]\n\n";
    std::cerr << "  n       - total number of coins (n >= 1)\n";
    std::cerr << "  k       - two-headed coins (0 <= k <= n)\n";
    std::cerr << "  trials  - Monte Carlo experiments (default: 1000000)\n";
}

bool parseArgs(int argc, char* argv[], InputData& in) {
    if (argc < 3 || argc > 4) {
        return false;
    }
    if (!parseU64(argv[1], in.n) || !parseU64(argv[2], in.k)) {
        return false;
    }
    if (argc == 4 && !parseU64(argv[3], in.trials)) {
        return false;
    }
    return true;
}

// P(4th is heads | first 3 are heads), analytical.
// P(first 3 heads) = (k/n)*1 + ((n-k)/n)*(1/8) = (7k + n) / (8n).
// P(first 3 heads and 4th head) = (k/n)*1 + ((n-k)/n)*(1/16) = (15k + n) / (16n).
// Аналитическая условная вероятность P(4-й орёл | 3 орла)
long double analyticalConditional(std::uint64_t n, std::uint64_t k) {
    const long double ldN = static_cast<long double>(n);
    const long double ldK = static_cast<long double>(k);
    const long double num = 15.0L * ldK + ldN;
    const long double den = 2.0L * (7.0L * ldK + ldN);
    return num / den;
}

}  // конец анонимного пространства имён

// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    InputData in;
    if (!parseArgs(argc, argv, in)) {
        printUsage(argv[0]);
        return 1;
    }
    if (in.n == 0) {
        std::cerr << "Error: n must be >= 1.\n";
        return 1;
    }
    if (in.k > in.n) {
        std::cerr << "Error: k must satisfy 0 <= k <= n.\n";
        return 1;
    }
    if (in.trials == 0) {
        std::cerr << "Error: trials must be >= 1.\n";
        return 1;
    }

    const long double pAnalytical = analyticalConditional(in.n, in.k);

    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<std::uint64_t> pickCoin(0, in.n - 1);
    std::bernoulli_distribution fairHeads(0.5);

    std::uint64_t countThreeHeads = 0;
    std::uint64_t countFourthHeadGivenThree = 0;

    // Монте-Карло: для каждого испытания выбираем монету и моделируем 4 броска
    for (std::uint64_t t = 0; t < in.trials; ++t) {
        const std::uint64_t coinIndex = pickCoin(rng);
        const bool twoHeaded = (coinIndex < in.k);

        bool h1 = false;
        bool h2 = false;
        bool h3 = false;
        bool h4 = false;
        if (twoHeaded) {
            h1 = h2 = h3 = h4 = true;
        } else {
            h1 = fairHeads(rng);
            h2 = fairHeads(rng);
            h3 = fairHeads(rng);
            h4 = fairHeads(rng);
        }

        if (h1 && h2 && h3) {
            ++countThreeHeads;
            if (h4) {
                ++countFourthHeadGivenThree;
            }
        }
    }

    long double pEmpirical = 0.0L;
    if (countThreeHeads > 0) {
        pEmpirical = static_cast<long double>(countFourthHeadGivenThree) /
                     static_cast<long double>(countThreeHeads);
    }

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "Wallet: n=" << in.n << " coins, k=" << in.k
              << " two-headed, " << (in.n - in.k) << " fair (one head, one tail).\n";
    std::cout << "Monte Carlo trials: " << in.trials << "\n\n";

    std::cout << "Question: P(4th flip is heads | first 3 flips are heads)\n\n";

    std::cout << "Analytical:\n";
    std::cout << "  P = (15k + n) / (2(7k + n)) = " << pAnalytical << "\n\n";

    std::cout << "Simulation (condition on first 3 heads in each trial):\n";
    std::cout << "  Trials with first 3 heads: " << countThreeHeads << "\n";
    std::cout << "  Among those, 4th also heads: " << countFourthHeadGivenThree << "\n";
    std::cout << "  P_hat = " << pEmpirical << "\n";
    if (countThreeHeads > 0) {
        std::cout << "  |P_hat - P_analytical| = " << std::fabs(pEmpirical - pAnalytical)
                  << "\n";
    } else {
        std::cout << "  (No trial had three heads in a row; increase trials.)\n";
    }

    return 0;
}
