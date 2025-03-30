#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <limits>
#include <algorithm>
#include <cmath>
#include <numeric>

struct Tier {
    double threshold;
    double fixedFee;
    double percent;
};

struct PiecewiseCommission {
    std::vector<Tier> tiers;

    double calculate(double amount) const {
        for (const auto& tier : tiers) {
            if (amount <= tier.threshold) {
                return tier.fixedFee + amount * tier.percent;
            }
        }
        const auto& lastTier = tiers.back();
        return lastTier.fixedFee + amount * lastTier.percent;
    }
};

struct Bank {
    std::string name;
    std::string country;
    PiecewiseCommission inputCommission;
    PiecewiseCommission outputCommission;
    std::vector<std::string> correspondents;

    bool hasCommonCorrespondent(const Bank& other) const {
        for (const auto& c : correspondents) {
            if (std::find(other.correspondents.begin(), other.correspondents.end(), c) != other.correspondents.end()) {
                return true;
            }
        }
        return false;
    }
};

struct BorderCommission {
    std::string fromCountry;
    std::string toCountry;
    double fee;
};

struct RouteOption {
    std::vector<std::string> path;
    bool guaranteed;
    double commission;
    int bestSplit = 1;
};

class TransferProblem {
public:
    std::map<std::string, Bank> banks;
    std::vector<BorderCommission> borderCommissions;
    double amount;
    std::string sourceBank;
    std::string destinationBank;

    double getBorderFee(const std::string& fromCountry, const std::string& toCountry) const {
        for (const auto& bc : borderCommissions) {
            if (bc.fromCountry == fromCountry && bc.toCountry == toCountry) {
                return bc.fee;
            }
        }
        return 0.0;
    }

    void loadFromCSV(const std::string& banksPath, const std::string& commissionsPath, const std::string& bordersPath) {
        loadBanks(banksPath);
        loadCommissions(commissionsPath);
        loadBorders(bordersPath);
    }

private:
    void loadBanks(const std::string& path) {
        std::ifstream file(path);
        std::string line;
        if (!file.is_open()) {
            std::cerr << "Не удалось открыть файл: " << path << "\n";
            return;
        }
        std::getline(file, line);
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string name, country, correspondentsStr;
            std::getline(ss, name, ',');
            std::getline(ss, country, ',');
            std::getline(ss, correspondentsStr);

            // Удаляем кавычки
            name.erase(std::remove(name.begin(), name.end(), '"'), name.end());
            country.erase(std::remove(country.begin(), country.end(), '"'), country.end());
            correspondentsStr.erase(std::remove(correspondentsStr.begin(), correspondentsStr.end(), '"'), correspondentsStr.end());

            Bank bank;
            bank.name = name;
            bank.country = country;

            std::stringstream corrSS(correspondentsStr);
            std::string corr;
            while (std::getline(corrSS, corr, ';')) {
                bank.correspondents.push_back(corr);
            }

            banks[name] = bank;
        }
    }

    void loadCommissions(const std::string& path) {
        std::ifstream file(path);
        std::string line;
        std::getline(file, line);
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string bankName, direction, thresholdStr, fixedStr, percentStr;
            std::getline(ss, bankName, ',');
            std::getline(ss, direction, ',');
            std::getline(ss, thresholdStr, ',');
            std::getline(ss, fixedStr, ',');
            std::getline(ss, percentStr);

            if (thresholdStr.empty() || fixedStr.empty() || percentStr.empty()) continue;

            try {
                Tier tier;
                tier.threshold = std::stod(thresholdStr);
                tier.fixedFee = std::stod(fixedStr);
                tier.percent = std::stod(percentStr);

                if (direction == "Input")
                    banks[bankName].inputCommission.tiers.push_back(tier);
                else if (direction == "Output")
                    banks[bankName].outputCommission.tiers.push_back(tier);
            } catch (const std::exception& e) {
                std::cerr << "Ошибка в строке комиссии: " << line << "\n";
                continue;
            }
        }
    }

    void loadBorders(const std::string& path) {
        std::ifstream file(path);
        std::string line;
        std::getline(file, line);
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string from, to, feeStr;
            std::getline(ss, from, ',');
            std::getline(ss, to, ',');
            std::getline(ss, feeStr);

            BorderCommission bc;
            bc.fromCountry = from;
            bc.toCountry = to;
            bc.fee = std::stod(feeStr);

            borderCommissions.push_back(bc);
        }
    }
};

class TransferSolver {
public:
    TransferSolver(const TransferProblem& p) : problem(p) {}

    RouteOption findBestRouteWithSplits() {
        const auto& A = problem.banks.at(problem.sourceBank);
        const auto& B = problem.banks.at(problem.destinationBank);
        std::vector<RouteOption> candidates;

        if (A.hasCommonCorrespondent(B)) {
            auto [minCost, split] = findMinCostWithSplits(problem.sourceBank, problem.destinationBank, problem.amount);
            candidates.push_back({{problem.sourceBank, problem.destinationBank}, true, minCost, split});
        }

        for (const auto& [name, C] : problem.banks) {
            if (name != A.name && name != B.name) {
                if (A.hasCommonCorrespondent(C) && C.hasCommonCorrespondent(B)) {
                    auto [costA_C, split1] = findMinCostWithSplits(A.name, C.name, problem.amount);
                    auto [costC_B, split2] = findMinCostWithSplits(C.name, B.name, problem.amount);
                    double totalCost = costA_C + costC_B;
                    int bestSplit = std::min(split1, split2);
                    candidates.push_back({{A.name, C.name, B.name}, true, totalCost, bestSplit});
                }
            }
        }

        auto best = std::min_element(candidates.begin(), candidates.end(), [](const RouteOption& a, const RouteOption& b) {
            return a.commission < b.commission;
        });

        return best != candidates.end() ? *best : RouteOption{{}, false, std::numeric_limits<double>::max()};
    }

private:
    const TransferProblem& problem;

    double calculateTotalCommission(const std::string& fromBank, const std::string& toBank, double amount) const {
        const auto& from = problem.banks.at(fromBank);
        const auto& to = problem.banks.at(toBank);

        double commission = 0.0;
        commission += from.outputCommission.calculate(amount);

        if (from.country != to.country) {
            commission += problem.getBorderFee(from.country, to.country);
        }

        commission += to.inputCommission.calculate(amount);
        return commission;
    }

    std::pair<double, int> findMinCostWithSplits(const std::string& fromBank, const std::string& toBank, double totalAmount) const {
        double bestCost = std::numeric_limits<double>::max();
        int bestSplit = 1;

        for (int parts = 1; parts <= 10; parts++) {
            double partAmount = totalAmount / parts;
            double totalCommission = 0.0;

            for (int i = 0; i < parts; i++) {
                totalCommission += calculateTotalCommission(fromBank, toBank, partAmount);
            }

            if (totalCommission < bestCost) {
                bestCost = totalCommission;
                bestSplit = parts;
            }
        }

        return {bestCost, bestSplit};
    }
};

int main() {
    std::cout << "Загрузка данных из CSV и расчет оптимального маршрута...\n";
    TransferProblem problem;
    problem.loadFromCSV("Banks.csv", "Commissions.csv", "Borders.csv");

    std::cout << "Загруженные банки:\n";
    for (const auto& [name, bank] : problem.banks) {
        std::cout << "- " << name << " (" << bank.country << ")\n";
    }

    std::cout << "Введите имя банка-отправителя: ";
    std::getline(std::cin, problem.sourceBank);

    std::cout << "Введите имя банка-получателя: ";
    std::getline(std::cin, problem.destinationBank);

    std::cout << "Введите сумму перевода: ";
    std::cin >> problem.amount;

    if (!problem.banks.count(problem.sourceBank)) {
        std::cerr << "Ошибка: банк-отправитель не найден.\n";
        return 1;
    }
    if (!problem.banks.count(problem.destinationBank)) {
        std::cerr << "Ошибка: банк-получатель не найден.\n";
        return 1;
    }

    TransferSolver solver(problem);
    RouteOption best = solver.findBestRouteWithSplits();

    std::cout << "Лучший маршрут: ";
    for (const auto& bank : best.path) {
        std::cout << bank << " ";
    }
    std::cout << "\nКомиссия: " << best.commission;
    std::cout << "\nГарантия: " << (best.guaranteed ? "Да" : "Нет");
    std::cout << "\nОптимальное количество частей: " << best.bestSplit << "\n";

    return 0;
}
