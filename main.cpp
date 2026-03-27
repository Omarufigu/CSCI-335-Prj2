#include "Compare.hpp"
#include "HashInventory.hpp"
#include "Inventory.hpp"
#include "Item.hpp"
#include "ItemAVL.hpp"
#include "ItemGenerator.hpp"
#include "TreeInventory.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <string>
#include <vector>

using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

struct ContainsRow {
    std::string inventory_type;
    int n;
    double avg_ms;
};

struct QueryRow {
    std::string inventory_type;
    std::string comparator;
    int n;
    double avg_ms;
};

template <class InventoryType>
InventoryType buildInventory(std::uint32_t seed, int n)
{
    InventoryType inv;
    ItemGenerator gen(seed);
    for (int i = 0; i < n; ++i) {
        inv.pickup(gen.randomItem());
    }
    return inv;
}

template <class InventoryType>
double averageContainsTime(InventoryType& inv, std::uint32_t seed, int n)
{
    ItemGenerator gen(seed);
    for (int i = 0; i < n; ++i) {
        inv.pickup(gen.randomItem());
    }

    std::vector<std::string> contained;
    std::vector<std::string> missing;
    contained.reserve(100);
    missing.reserve(100);

    for (int i = 0; i < 100; ++i) {
        contained.push_back(gen.randomUsedName());
    }
    for (int i = 0; i < 100; ++i) {
        missing.push_back(gen.randomItem().name_);
    }

    double total_ms = 0.0;

    for (const auto& name : contained) {
        auto start = Clock::now();
        volatile bool found = inv.contains(name);
        (void)found;
        auto end = Clock::now();
        total_ms += Ms(end - start).count();
    }

    for (const auto& name : missing) {
        auto start = Clock::now();
        volatile bool found = inv.contains(name);
        (void)found;
        auto end = Clock::now();
        total_ms += Ms(end - start).count();
    }

    return total_ms / 200.0;
}

template <class InventoryType>
double averageNameQueryTime(InventoryType& inv, std::uint32_t seed, int n)
{
    ItemGenerator gen(seed);
    for (int i = 0; i < n; ++i) {
        inv.pickup(gen.randomItem());
    }

    double total_ms = 0.0;
    for (int i = 0; i < 10; ++i) {
        std::string first = gen.randomUsedName();
        std::string second = gen.randomUsedName();
        Item start(first, 0.0f, NONE);
        Item end(second, 0.0f, NONE);
        if (CompareItemName::lessThan(end, start)) {
            std::swap(start, end);
        }

        auto t1 = Clock::now();
        volatile auto result = inv.query(start, end);
        (void)result;
        auto t2 = Clock::now();
        total_ms += Ms(t2 - t1).count();
    }

    return total_ms / 10.0;
}

template <class InventoryType>
double averageWeightQueryTime(InventoryType& inv, std::uint32_t seed, int n)
{
    ItemGenerator gen(seed);
    for (int i = 0; i < n; ++i) {
        inv.pickup(gen.randomItem());
    }

    double total_ms = 0.0;
    for (int i = 0; i < 10; ++i) {
        float w = gen.randomFloat(ItemGenerator::MIN_WEIGHT, ItemGenerator::MAX_WEIGHT);
        Item start("low", w, NONE);
        Item end("high", w + 0.1f, NONE);

        auto t1 = Clock::now();
        volatile auto result = inv.query(start, end);
        (void)result;
        auto t2 = Clock::now();
        total_ms += Ms(t2 - t1).count();
    }

    return total_ms / 10.0;
}

void writeContainsTable(const std::vector<ContainsRow>& rows, std::ostream& os)
{
    os << "contains() average time in milliseconds\n";
    os << std::left << std::setw(18) << "Inventory" << std::setw(8) << "n" << std::setw(14) << "avg_ms" << "\n";
    os << std::string(40, '-') << "\n";
    for (const auto& row : rows) {
        os << std::left << std::setw(18) << row.inventory_type
           << std::setw(8) << row.n
           << std::setw(14) << std::fixed << std::setprecision(6) << row.avg_ms << "\n";
    }
    os << "\n";
}

void writeQueryTable(const std::vector<QueryRow>& rows, std::ostream& os)
{
    os << "query() average time in milliseconds\n";
    os << std::left << std::setw(18) << "Inventory" << std::setw(14) << "Comparator" << std::setw(8) << "n" << std::setw(14) << "avg_ms" << "\n";
    os << std::string(54, '-') << "\n";
    for (const auto& row : rows) {
        os << std::left << std::setw(18) << row.inventory_type
           << std::setw(14) << row.comparator
           << std::setw(8) << row.n
           << std::setw(14) << std::fixed << std::setprecision(6) << row.avg_ms << "\n";
    }
    os << "\n";
}

int main()
{
    const std::vector<int> sizes { 1000, 2000, 4000, 8000 };
    const std::uint32_t seed = 42;

    std::vector<ContainsRow> contains_rows;
    std::vector<QueryRow> query_rows;

    for (int n : sizes) {
        {
            Inventory<CompareItemName> inv;
            contains_rows.push_back({ "vector", n, averageContainsTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemName, std::list<Item>> inv;
            contains_rows.push_back({ "list", n, averageContainsTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemName, std::unordered_set<Item>> inv;
            contains_rows.push_back({ "hash", n, averageContainsTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemName, Tree> inv;
            contains_rows.push_back({ "tree", n, averageContainsTime(inv, seed, n) });
        }

        {
            Inventory<CompareItemName> inv;
            query_rows.push_back({ "vector", "name", n, averageNameQueryTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemName, std::list<Item>> inv;
            query_rows.push_back({ "list", "name", n, averageNameQueryTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemName, std::unordered_set<Item>> inv;
            query_rows.push_back({ "hash", "name", n, averageNameQueryTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemName, Tree> inv;
            query_rows.push_back({ "tree", "name", n, averageNameQueryTime(inv, seed, n) });
        }

        {
            Inventory<CompareItemWeight> inv;
            query_rows.push_back({ "vector", "weight", n, averageWeightQueryTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemWeight, std::list<Item>> inv;
            query_rows.push_back({ "list", "weight", n, averageWeightQueryTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemWeight, std::unordered_set<Item>> inv;
            query_rows.push_back({ "hash", "weight", n, averageWeightQueryTime(inv, seed, n) });
        }
        {
            Inventory<CompareItemWeight, Tree> inv;
            query_rows.push_back({ "tree", "weight", n, averageWeightQueryTime(inv, seed, n) });
        }
    }

    writeContainsTable(contains_rows, std::cout);
    writeQueryTable(query_rows, std::cout);

    std::ofstream report("timing_report.txt");
    writeContainsTable(contains_rows, report);
    writeQueryTable(query_rows, report);
    report << "Analysis:\n";
    report << "The hash inventory was the fastest for contains() because unordered_set supports expected constant-time lookup by item name. "
              "The vector and list versions were slower because they scan linearly through the container. "
              "The AVL-based tree was also slower for contains() than a normal tree search would usually be, because this project requires uniqueness by name even when the tree is ordered by some other comparator, so contains() must traverse the whole tree in the worst case. "
              "For query(), the vector, list, and hash inventories all behave like linear scans, while the tree inventory can prune subtrees that fall completely outside the requested range, so it should be the best choice when range queries are common. "
              "Use the hash inventory when fast membership checks and exact-name operations matter most, and use the tree inventory when efficient ordered range queries matter more than exact lookup speed.\n";

    return 0;
}
