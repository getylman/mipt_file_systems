#include "b-tree.hpp"

#include <cassert>
#include <iostream>
#include <map>
#include <random>
#include <thread>
#include <unordered_map>

std::vector<int> fff(const std::map<int, int>& a) {
    std::vector<int> b;
    for (auto [x, y] : a) {
        b.push_back(x);
    }
    return b;
}

void SimpleTest(void) {
    std::map<int, int> a;
    std::unordered_map<int, int> b;
    std::random_device rd; // Используем для генерации начального значения
    std::mt19937 generator(rd()); // Генератор случайных чисел
    std::uniform_int_distribution<> distribution(1, 100); // Распределение случайных чисел

    btree::BLinkTree<int, int, 2> t;

    auto v = t.GetValues();
    auto va = fff(a);

    // fill test
    for (int i = 90; i > 1; --i) {
        const auto x = distribution(generator);
        const auto y = distribution(generator);
        // std::cout << "id is "<< i << " current x y are: " << x << " and " << y << '\n';
        const auto contained = t.contains(x);
        a.insert({x, y});
        b.insert({x, y});
        t.insert({x, y});
        va = fff(a);
        v = t.GetValues();
        assert(t.contains(x));
        // std::cout << t.at(x) << " is eq to " << y << " where x is " << x << '\n';
        if (!contained) {
            assert((t.at(x) == y)); 
        }
        assert(va == v);
        // for (auto i : v) {std::cout << i << ' ';} std::cout << '\n';
    }

    auto ta = t.GetValues();

    for (auto [key, value] : a) {
        std::cout << key << ' ';
    } std::cout << '\n';
    for (auto i : ta) {
        std::cout << i << ' ';
    } std::cout << '\n';

    std::cout << t.size() << ' ' << a.size() << '\n';
} 

void SimpleTestPlusRm(void) {
    std::map<int, int> a;
    std::unordered_map<int, int> b;
    std::random_device rd; // Используем для генерации начального значения
    std::mt19937 generator(rd()); // Генератор случайных чисел
    std::uniform_int_distribution<> distribution(1, 100); // Распределение случайных чисел

    btree::BLinkTree<int, int, 2> t;

    auto v = t.GetValues();
    auto va = fff(a);

    // fill test
    for (int i = 20; i > 1; --i) {
        const auto x = distribution(generator);
        const auto y = distribution(generator);
        // std::cout << "id is "<< i << " current x y are: " << x << " and " << y << '\n';
        const auto contained = t.contains(x);
        a.insert({x, y});
        b.insert({x, y});
        t.insert({x, y});
        va = fff(a);
        v = t.GetValues();
        assert(t.contains(x));
        // std::cout << t.at(x) << " is eq to " << y << " where x is " << x << '\n';
        if (!contained) {
            assert((t.at(x) == y)); 
        }
        assert(va == v);
        // for (auto i : v) {std::cout << i << ' ';} std::cout << '\n';
    }

    auto ta = t.GetValues();

    for (auto [key, value] : a) {
        std::cout << key << ' ';
    } std::cout << '\n';
    for (auto i : ta) {
        std::cout << i << ' ';
    } std::cout << '\n';

    std::cout << t.size() << ' ' << a.size() << '\n';

    for (const auto& [key, value] : b) {
        assert(t.contains(key));
        std::cout << key << '\t' << t.at(key) << '\n';
        assert(t.at(key) == value);
        t.erase(key);
        assert(!t.contains(key));
    }
} 

void Subtest(void) {
    std::map<int, int> a;
    std::unordered_map<int, int> b;
    std::random_device rd; // Используем для генерации начального значения
    std::mt19937 generator(rd()); // Генератор случайных чисел
    std::uniform_int_distribution<> distribution(1, 100); // Распределение случайных чисел

    btree::BLinkTree<int, int, 2> t;

    auto v = t.GetValues();
    auto va = fff(a);

    // fill test
    for (int i = 20; i > 1; --i) {
        const auto x = i;//distribution(generator);
        const auto y = i;//distribution(generator);
        // std::cout << "id is "<< i << " current x y are: " << x << " and " << y << '\n';
        const auto contained = t.contains(x);
        a.insert({x, y});
        b.insert({x, y});
        t.insert({x, y});
        va = fff(a);
        v = t.GetValues();
        assert(t.contains(x));
        // std::cout << t.at(x) << " is eq to " << y << " where x is " << x << '\n';
        if (!contained) {
            assert((t.at(x) == y)); 
        }
        assert(va == v);
        // for (auto i : v) {std::cout << i << ' ';} std::cout << '\n';
    }

    auto ta = t.GetValues();

    for (auto [key, value] : a) {
        std::cout << key << ' ';
    } std::cout << '\n';
    for (auto i : ta) {
        std::cout << i << ' ';
    } std::cout << '\n';

    std::cout << t.size() << ' ' << a.size() << '\n';

    const std::vector<std::pair<int, int>> erase_test{
      {  2,	2},
      {  3,	3},
      {  4,	4},
      {  5,	5},
      {  6,	6},
      {  7,	7},
        {20	,20},
        {19	,19},
        {18	,18},
        {17	,17}
        // {16	,16}
    };

    for (const auto& [key, value] : erase_test) {
        assert(t.contains(key));
        std::cout << key << '\t' << t.at(key) << '\n';
        assert(t.at(key) == value);
        t.erase(key);
        assert(!t.contains(key));
    }

    t.erase(16);  // TODO:
} 

std::map<int, int> a;
btree::BLinkTree<int, int, 2> t;
std::mutex mtx;

void SimpleTestMultiInsertTask(int id, int num) {
    for (int i = id; i < 25; i += num) {
        t.insert({i, i});
        mtx.lock();
        a.insert({i, i});
        // const auto aa = t.GetValues();
        mtx.unlock();
    }
}

void SimpleTestMultiInsert(void) {
    // std::map<int, int> a;
    std::unordered_map<int, int> b;
    std::random_device rd; // Используем для генерации начального значения
    std::mt19937 generator(rd()); // Генератор случайных чисел
    std::uniform_int_distribution<> distribution(1, 100); // Распределение случайных чисел

    // btree::BLinkTree<int, int, 2> t;

    auto v = t.GetValues();
    auto va = fff(a);

    // fill test
    const int num = 2;
    std::vector<std::thread> threads;
    // std::mutex mtx;

    for (int i = 0; i < num; ++i) {
        threads.emplace_back(SimpleTestMultiInsertTask, i, num);
    }
    
    
    for (auto& i : threads) {
        i.join();
    }

    auto ta = t.GetValues();

    for (auto [key, value] : a) {
        std::cout << key << ' ';
    } std::cout << '\n';
    for (auto i : ta) {
        std::cout << i << ' ';
    } std::cout << '\n';

    std::cout << t.size() << ' ' << a.size() << '\n';
} 

int main() {
    // SimpleTestPlusRm();
    SimpleTest();
    // Subtest();
    // SimpleTestMultiInsert();
    return 0;
}

// std::map<int, int> foo(btree::BLinkTree<int, int, 2>& t) {
//     std::map<int, int> b;
//     auto a = t.GetValues();
//     for (int i = 1; i < 12; ++i) {
//         b.insert({((i % 2 == 0) ? 1 : -1) * (16 - i), i});
//         t.insert({((i % 2 == 0) ? 1 : -1) * (16 - i), i});
//         a = t.GetValues();
//     }
    
//     for (int i = 12; i < 15; ++i) {
//         b.insert({((i % 2 == 0) ? 1 : -1) * (16 - i), i});
//         t.insert({((i % 2 == 0) ? 1 : -1) * (16 - i), i});
//         a = t.GetValues();
//     }
//     return b;
// }



// std::map<int, int> goo(btree::BLinkTree<int, int, 2>& t) {
//     std::map<int, int> a;
//     std::random_device rd; // Используем для генерации начального значения
//     std::mt19937 generator(rd()); // Генератор случайных чисел
//     std::uniform_int_distribution<> distribution(1, 100); // Распределение случайных чисел

//     auto v = t.GetValues();
//     auto va = fff(a);

//     for (int i = 30; i > 5; --i) {
//         const auto x = distribution(generator);
//         const auto y = distribution(generator);
//         a.insert({x, y});
//         t.insert({x, y});
//         va = fff(a);
//         v = t.GetValues();
//         for (auto i : v) {std::cout << i << ' ';} std::cout << '\n';
//     }
//     const auto x = distribution(generator);
//         const auto y = distribution(generator);
//         a.insert({x, y});
//         t.insert({x, y});
//         v = t.GetValues();
//         for (auto i : v) {std::cout << i << ' ';} std::cout << '\n';
//     for (int i = 4; i > 0; --i) {
//         // false program stops at locking mutex after geting leaf
//         const auto x = distribution(generator);
//         const auto y = distribution(generator);
//         a.insert({x, y});
//         t.insert({x, y});
//         va = fff(a);
//         v = t.GetValues();
//         for (auto i : v) {std::cout << i << ' ';} std::cout << '\n';
//     }

//     return a;
// }

// void Rm(btree::BLinkTree<int, int, 2>& t, int x) {
//     std::cout << t.at(x) << '\n';

//     t.erase(x);

//     auto ta = t.GetValues();

//     std::cout << t.contains(x) << '\n';

//     for (auto i : ta) {
//         std::cout << i << ' ';
//     } std::cout << '\n';
// }

// void rmTest() {
//     btree::BLinkTree<int, int, 2> t;

//     const auto a = foo(t);

//     auto ta = t.GetValues();

//     for (auto [key, value] : a) {
//         std::cout << key << ' ';
//     } std::cout << '\n';
//     for (auto i : ta) {
//         std::cout << i << ' ';
//     } std::cout << '\n';

//     std::cout << t.size() << ' ' << a.size() << '\n';

//     Rm(t, -15);
//     Rm(t, -13);
//     Rm(t, -11);
//     Rm(t, -9);
//     Rm(t, -7);
//     Rm(t, -3);
//     Rm(t, 2);
//     Rm(t, 4);
//     Rm(t, 6);
//     Rm(t, 8);
//     Rm(t, 10);
//     Rm(t, 12);
//     Rm(t, 14);
//     Rm(t, -5);
// }



// void rmTest2() {
//     btree::BLinkTree<int, int, 2> t;

//     const auto a = foo(t);

//     auto ta = t.GetValues();

//     for (auto [key, value] : a) {
//         std::cout << key << ' ';
//     } std::cout << '\n';
//     for (auto i : ta) {
//         std::cout << i << ' ';
//     } std::cout << '\n';

//     std::cout << t.size() << ' ' << a.size() << '\n';

//     Rm(t, 14);
//     Rm(t, 12);
//     Rm(t, 10);
//     Rm(t, 8);
//     Rm(t, 6);
//     Rm(t, 4);
//     Rm(t, 2);
//     Rm(t, -3);
//     Rm(t, -5);
//     Rm(t, -7);
//     Rm(t, -9);
//     Rm(t, -11);
//     Rm(t, -13);
//     Rm(t, -15);
// }
// void SubTest() {
//     btree::BLinkTree<int, int, 2> t;

//     const std::vector<std::pair<int, int>> test_data{
//         {94, 67},
//         {7, 24},
//         {3, 57},
//         {84, 85},
//         {68, 46},
//         {29, 64},
//         {93, 39},
//         {24, 16},
//         {37, 54},
//         {73, 54},
//         {7, 41},
//         {2, 25}
//     };

//     for (const auto& [key, value]: test_data) {
//         std::cout << key << '\t' << value << '\t';
//         t.insert({key, value});
//         std::cout << "at value is " << t.at(key) << '\n';
//     }
    
//     for (auto i : t.GetValues()) {
//         std::cout << i << ' ';
//     }
//     // t.insert({7, 41});
//     std::cout << "\nmake insert " << std::endl;
//     // t.insert({2, 25});
//     std::cout << "first done" << std::endl;
//     std::cout << t.at(2) << " done\n";
// }

// void SubTest2() {
//     btree::BLinkTree<int, int, 2> t;

//     const std::vector<std::pair<int, int>> test_data{
//         {46, 5},
//         {71, 77},
//         {17, 55},
//         {65, 27},
//         {81, 21},
//         {30, 8},
//         {44, 75},
//         {46, 67},
//         {6, 23},
//         {19, 27},
//         {55, 98},
//         {41, 79},
//         {44, 88},
//         {75, 22},
//         {76, 3},
//         {28, 12}
//         // {98, 17},
//         // {89, 28}
//         // {49, 24},
//         // {43, 72}
//     };

//     const std::vector<std::pair<int, int>> test_data2{
//         // {46, 5},
//         // {71, 77},
//         // {17, 55},
//         // {65, 27},
//         // {81, 21},
//         // {30, 8},
//         // {44, 75},
//         // {46, 67},
//         // {6, 23},
//         // {19, 27},
//         // {55, 98},
//         // {41, 79},
//         // {44, 88},
//         // {75, 22},
//         // {76, 3},
//         // {28, 12},
//         // {98, 17},
//         {89, 28}
//         // {49, 24},
//         // {43, 72}
//     };


//     for (const auto& [key, value]: test_data) {
//         std::cout << key << '\t' << value << '\t';
//         t.insert({key, value});
//         std::cout << "at value is " << t.at(key) << '\n';
//     }

//     t.insert({98, 0});

//     for (const auto& [key, value]: test_data2) {
//         std::cout << key << '\t' << value << '\t';
//         t.insert({key, value});
//         std::cout << "at value is " << t.at(key) << '\n';
//     }
    
//     for (auto i : t.GetValues()) {
//         std::cout << i << ' ';
//     }
//     t.insert({49, 24});
//     std::cout << "\nmake insert " << std::endl;
//     t.insert({43, 72});
//     std::cout << "first done" << std::endl;
// }

// void SubTest1() {
//     btree::BLinkTree<int, int, 2> t;

//     for (int i = 0; i < 100; ++i) {
//         std::cout << i << '\t';
//         t.insert({i, i});
//         std::cout << t.at(i) << '\n';
//     }

//     for (auto i : t.GetValues()) {
//         std::cout << i << ' ';
//     } std::cout << '\n';
// }


// void SubTest3() {
//     btree::BLinkTree<int, int, 2> t;

//     const std::vector<std::pair<int, int>> test_data{
//         {36, 37},
//         {30, 10},
//         {5, 45},
//         {60, 37},
//         {99, 86},
//         {43, 6},
//         {87, 77},
//         {42, 59},
//         {72, 92},
//         {40, 63},
//         {75, 8},
//         {38, 62},
//         {88, 75},
//         {61, 1},
//         {70, 84},
//         {52, 19},
//         {78, 96},
//         {90, 82},
//         {84, 100},
//         {65, 41},
//         {88, 89},
//         {95, 35},
//         {89, 48},
//         {54, 7}
//         // {80, 95}
//     };

//     for (const auto& [key, value]: test_data) {
//         std::cout << key << '\t' << value << '\t';
//         t.insert({key, value});
//         std::cout << "at value is " << t.at(key) << '\n';
//     }
    
//     for (auto i : t.GetValues()) {
//         std::cout << i << ' ';
//     }
//     // t.insert({7, 41});
//     std::cout << "\nmake insert " << std::endl;
//     t.insert({80, 95});
//     for (auto i : t.GetValues()) {
//         std::cout << i << ' ';
//     }
//     std::cout << "first done" << std::endl;
//     // std::cout << t.at(2) << " done\n";
// }