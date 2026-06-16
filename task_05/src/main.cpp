#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
// Задание 5. Случайное блуждание с JSON-конфигурацией (вариант 3)
// Параметры в config.json; оценка P(возврат в 0 на шаге N)
// Запуск: ./task_05 [config.json]
// =============================================================================

// Анонимное пространство имён — внутренние функции модуля
namespace {

// Параметры блуждания из config.json
struct Config {
    long double p = 0.5L;
    std::uint64_t sPlus = 1;
    std::uint64_t sMinus = 1;
    std::uint64_t trials = 100000;
    std::vector<std::uint64_t> targetN;
};

std::string readTextFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open config file: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool extractNumber(const std::string& json, const std::string& key, std::string& out) {
    const std::regex re("\"" + key + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, re)) {
        return false;
    }
    out = match[1].str();
    return true;
}

bool extractArray(const std::string& json, const std::string& key, std::string& out) {
    const std::regex re("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch match;
    if (!std::regex_search(json, match, re)) {
        return false;
    }
    out = match[1].str();
    return true;
}

// Парсинг JSON-конфига (без внешних библиотек)
Config parseConfig(const std::string& path) {
    const std::string json = readTextFile(path);
    Config cfg;

    std::string numberText;
    if (!extractNumber(json, "p", numberText)) {
        throw std::runtime_error("Missing required key: p");
    }
    cfg.p = std::stold(numberText);

    if (!extractNumber(json, "s_plus", numberText)) {
        throw std::runtime_error("Missing required key: s_plus");
    }
    cfg.sPlus = static_cast<std::uint64_t>(std::stoull(numberText));

    if (!extractNumber(json, "s_minus", numberText)) {
        throw std::runtime_error("Missing required key: s_minus");
    }
    cfg.sMinus = static_cast<std::uint64_t>(std::stoull(numberText));

    if (!extractNumber(json, "trials", numberText)) {
        throw std::runtime_error("Missing required key: trials");
    }
    cfg.trials = static_cast<std::uint64_t>(std::stoull(numberText));

    std::string arrText;
    if (!extractArray(json, "target_N", arrText)) {
        throw std::runtime_error("Missing required key: target_N");
    }
    const std::regex numRe("([0-9]+)");
    for (auto it = std::sregex_iterator(arrText.begin(), arrText.end(), numRe);
         it != std::sregex_iterator(); ++it) {
        const auto n = static_cast<std::uint64_t>(std::stoull((*it)[1].str()));
        cfg.targetN.push_back(n);
    }

    if (cfg.targetN.empty()) {
        throw std::runtime_error("target_N must contain at least one value.");
    }
    if (cfg.p < 0.0L || cfg.p > 1.0L) {
        throw std::runtime_error("p must be in [0, 1].");
    }
    if (cfg.sPlus == 0 || cfg.sMinus == 0) {
        throw std::runtime_error("s_plus and s_minus must be positive.");
    }
    if (cfg.trials == 0) {
        throw std::runtime_error("trials must be positive.");
    }
    constexpr std::uint64_t maxAllowedN = (1ULL << 32);
    for (const auto n : cfg.targetN) {
        if (n == 0) {
            throw std::runtime_error("All N in target_N must be >= 1.");
        }
        if (n > maxAllowedN) {
            throw std::runtime_error("All N in target_N must satisfy N <= 2^32.");
        }
    }

    std::sort(cfg.targetN.begin(), cfg.targetN.end());
    cfg.targetN.erase(std::unique(cfg.targetN.begin(), cfg.targetN.end()), cfg.targetN.end());

    return cfg;
}

void printUsage(const char* exeName) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << exeName << " [config_path]\n\n";
    std::cerr << "Default config path: config.json\n";
}

}  // конец анонимного пространства имён

// --- Точка входа программы ---
int main(int argc, char* argv[]) {
    if (argc > 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string configPath = (argc == 2) ? argv[1] : "config.json";

    Config cfg;
    try {
        cfg = parseConfig(configPath);
    } catch (const std::exception& ex) {
        std::cerr << "Config parse error: " << ex.what() << "\n";
        return 1;
    }

    const long double q = 1.0L - cfg.p;
    const std::uint64_t maxN = cfg.targetN.back();
    std::vector<std::uint64_t> hitCounts(cfg.targetN.size(), 0);

    std::mt19937_64 rng(std::random_device{}());
    std::bernoulli_distribution stepRight(static_cast<double>(cfg.p));

    // Цикл Монте-Карло: серия блужданий до cfg.trials испытаний
    for (std::uint64_t t = 0; t < cfg.trials; ++t) {
        long long x = 0;
        std::size_t idx = 0;
        for (std::uint64_t step = 1; step <= maxN; ++step) {
            if (stepRight(rng)) {
                x += static_cast<long long>(cfg.sPlus);
            } else {
                x -= static_cast<long long>(cfg.sMinus);
            }

            while (idx < cfg.targetN.size() && cfg.targetN[idx] == step) {
                if (x == 0) {
                    ++hitCounts[idx];
                }
                ++idx;
            }
            if (idx >= cfg.targetN.size()) {
                break;
            }
        }
    }

    std::cout << std::fixed << std::setprecision(10);
    std::cout << "Config: p=" << cfg.p << ", q=" << q
              << ", s_plus=" << cfg.sPlus
              << ", s_minus=" << cfg.sMinus
              << ", trials=" << cfg.trials << "\n";
    std::cout << "Target N values: ";
    for (std::size_t i = 0; i < cfg.targetN.size(); ++i) {
        std::cout << cfg.targetN[i];
        if (i + 1 < cfg.targetN.size()) {
            std::cout << ", ";
        }
    }
    std::cout << "\n\n";

    std::cout << "Empirical probabilities P(X_N = 0):\n";
    for (std::size_t i = 0; i < cfg.targetN.size(); ++i) {
        const long double prob = static_cast<long double>(hitCounts[i]) /
                                 static_cast<long double>(cfg.trials);
        std::cout << "  N=" << cfg.targetN[i]
                  << "  hits=" << hitCounts[i]
                  << "  P_hat=" << prob << "\n";
    }

    return 0;
}
