#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Задание 4. Подбор ключа из связки (вариант 3)
// Случайный порядок n ключей; оценка P(T=m)=1/n
// Запуск: ./task_04 n K
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

struct InputData {
    int n = 0;               // Число ключей
    std::uint64_t k = 0;     // Число экспериментов
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
    std::cerr << "  " << exeName << " n K\n\n";
    std::cerr << "Where:\n";
    std::cerr << "  n - number of keys in bundle (n >= 1)\n";
    std::cerr << "  K - number of experiments in series (K >= 1)\n";
}

bool parseArgs(int argc, char* argv[], InputData& in) {
    if (argc != 3) {
        return false;
    }
    if (!parsePositiveInt(argv[1], in.n)) {
        return false;
    }
    if (!parsePositiveU64(argv[2], in.k)) {
        return false;
    }
    return true;
}

// Один эксперимент: shuffle ключей, ищем позицию ключа №1
int runSingleExperiment(int n, std::mt19937_64& rng) {
    // Позиция правильного ключа в случайной перестановке n ключей.
    // Это и есть номер попытки, на которой ключ подошёл.
    std::vector<int> keys(n);
    std::iota(keys.begin(), keys.end(), 1);  // ключ №1 — правильный
    std::shuffle(keys.begin(), keys.end(), rng);

    for (int attempt = 1; attempt <= n; ++attempt) {
        if (keys[attempt - 1] == 1) {
            return attempt;
        }
    }
    return n;  // недостижимо, оставлено для надёжности
}

}  // конец анонимного пространства имён

// --- Серия из K экспериментов; гистограмма P(T=m) ---
// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    InputData in;
    if (!parseArgs(argc, argv, in)) {
        printUsage(argv[0]);
        return 1;
    }

    std::mt19937_64 rng(std::random_device{}());
    std::vector<std::uint64_t> outcomeCount(static_cast<std::size_t>(in.n + 1), 0);

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "Input: n=" << in.n << ", K=" << in.k << "\n";
    std::cout << "Analytical statement: P(T = m) = 1/n for each m in {1,...,n}\n\n";
    std::cout << "Series of experiments:\n";

    for (std::uint64_t exp = 1; exp <= in.k; ++exp) {
        const int attempts = runSingleExperiment(in.n, rng);
        ++outcomeCount[static_cast<std::size_t>(attempts)];
        std::cout << "  Experiment #" << exp << ": success on attempt " << attempts << "\n";
    }

    std::cout << "\nOutcome probabilities:\n";
    const long double analytical = 1.0L / static_cast<long double>(in.n);
    for (int m = 1; m <= in.n; ++m) {
        const long double empirical = static_cast<long double>(outcomeCount[m]) /
                                      static_cast<long double>(in.k);
        const long double diff = std::fabs(empirical - analytical);
        std::cout << "  T=" << m
                  << "  count=" << outcomeCount[m]
                  << "  empirical=" << empirical
                  << "  analytical=" << analytical
                  << "  |diff|=" << diff << "\n";
    }

    long double sumEmpirical = 0.0L;
    for (int m = 1; m <= in.n; ++m) {
        sumEmpirical += static_cast<long double>(outcomeCount[m]) /
                        static_cast<long double>(in.k);
    }
    std::cout << "\nCheck: sum of empirical probabilities = " << sumEmpirical << "\n";

    return 0;
}
