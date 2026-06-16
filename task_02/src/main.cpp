#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Задание 2. Монета до двух одинаковых подряд (вариант 3)
// Симуляция до пары ОО/РР; сравнение Монте-Карло с аналитикой P(T<k), P(T чётное)
// Запуск: ./task_02 k [trials]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// --- Параметры: порог k и число испытаний ---
struct InputData {
    int k = 0;
    std::uint64_t trials = 200000;
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

bool parsePositiveU64(const char* text, std::uint64_t& value) {
    try {
        std::string s(text);
        std::size_t pos = 0;
        const unsigned long long parsed = std::stoull(s, &pos);
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
    std::cerr << "  " << exeName << " k [trials]\n\n";
    std::cerr << "Where:\n";
    std::cerr << "  k       - threshold toss index (k >= 1)\n";
    std::cerr << "  trials  - number of Monte-Carlo experiments (optional)\n";
}

bool parseArgs(int argc, char* argv[], InputData& in) {
    if (argc < 2 || argc > 3) {
        return false;
    }
    if (!parsePositiveInt(argv[1], in.k)) {
        return false;
    }
    if (argc == 3 && !parsePositiveU64(argv[2], in.trials)) {
        return false;
    }
    return true;
}

// Построение примеров элементарных исходов Ω
std::vector<std::string> buildElementaryOutcomesPrefix(int maxLength) {
    // Ω = {ОО, РР, ОРР, РОО, ...} — элементарные исходы по времени остановки T
    // Каждый исход соответствует моменту остановки T = n.
    std::vector<std::string> outcomes;
    for (int n = 2; n <= maxLength; ++n) {
        std::string seqH;
        std::string seqT;
        seqH.reserve(static_cast<std::size_t>(n));
        seqT.reserve(static_cast<std::size_t>(n));

        // Начинаем с О/Р и чередуем до позиции n-1.
        char curH = 'H';
        char curT = 'T';
        for (int pos = 1; pos <= n - 1; ++pos) {
            seqH.push_back(curH);
            seqT.push_back(curT);
            curH = (curH == 'H') ? 'T' : 'H';
            curT = (curT == 'H') ? 'T' : 'H';
        }
        // Последний бросок совпадает с предыдущим — условие остановки.
        seqH.push_back(seqH.back());
        seqT.push_back(seqT.back());

        outcomes.push_back(seqH);
        outcomes.push_back(seqT);
    }
    return outcomes;
}

// Одно испытание: бросаем монету до пары одинаковых
int runSingleExperiment(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> coin(0, 1);  // 0=Р, 1=О
    int prev = coin(rng);
    int tosses = 1;

    while (true) {
        const int cur = coin(rng);
        ++tosses;
        if (cur == prev) {
            return tosses;
        }
        prev = cur;
    }
}

// Аналитика: P(T < k) = 1 - 1/2^(k-2)
long double analyticPStopBeforeKStrict(int k) {
    // Событие A: T < k.
    // P(T=n) = 1/2^(n-1), n ≥ 2.
    if (k <= 2) {
        return 0.0L;
    }
    // Σ_{n=2}^{k-1} 1/2^(n-1) = 1 - 1/2^(k-2).
    const long double tail = std::pow(2.0L, static_cast<long double>(-(k - 2)));
    return 1.0L - tail;
}

// Аналитика: P(T чётное) = 2/3
long double analyticPStopEven() {
    // P(T чётное) = Σ_{m≥1} P(T=2m)
    //           = Σ_{m≥1} 1/2^(2m-1)
    //           = (1/2)/(1-1/4) = 2/3.
    return 2.0L / 3.0L;
}

}  // конец анонимного пространства имён

// --- Монте-Карло и сравнение с формулами ---
// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    InputData in;
    if (!parseArgs(argc, argv, in)) {
        printUsage(argv[0]);
        return 1;
    }

    std::mt19937_64 rng(std::random_device{}());

    std::uint64_t countA = 0;  // A: T < k
    std::uint64_t countB = 0;  // B: T чётное

    for (std::uint64_t t = 0; t < in.trials; ++t) {
        const int stopTime = runSingleExperiment(rng);
        if (stopTime < in.k) {
            ++countA;
        }
        if (stopTime % 2 == 0) {
            ++countB;
        }
    }

    const long double empiricalA =
        static_cast<long double>(countA) / static_cast<long double>(in.trials);
    const long double empiricalB =
        static_cast<long double>(countB) / static_cast<long double>(in.trials);

    const long double analyticA = analyticPStopBeforeKStrict(in.k);
    const long double analyticB = analyticPStopEven();

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "Input: k=" << in.k << ", trials=" << in.trials << "\n\n";

    std::cout << "Elementary outcomes space (prefix):\n";
    std::cout << "  Omega = {HH, TT, HTT, THH, HTHH, THTT, ...}\n";
    const int previewLength = std::min(in.k + 2, 10);
    const auto outcomes = buildElementaryOutcomesPrefix(previewLength);
    std::cout << "  First generated outcomes by stop time T=2.." << previewLength << ":\n";
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        std::cout << "    " << outcomes[i];
        if (i + 1 < outcomes.size()) {
            std::cout << ",";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    std::cout << "Event A: experiment ends before k-th toss (T < k)\n";
    std::cout << "  Analytical P(A) = " << analyticA << "\n";
    std::cout << "  Empirical  P(A) = " << empiricalA << "  (" << countA << "/" << in.trials
              << ")\n";
    std::cout << "  |diff|           = " << std::fabs(analyticA - empiricalA) << "\n\n";

    std::cout << "Event B: experiment ends in an even number of tosses\n";
    std::cout << "  Analytical P(B) = " << analyticB << "\n";
    std::cout << "  Empirical  P(B) = " << empiricalB << "  (" << countB << "/" << in.trials
              << ")\n";
    std::cout << "  |diff|           = " << std::fabs(analyticB - empiricalB) << "\n";

    return 0;
}
