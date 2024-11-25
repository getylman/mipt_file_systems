#include <cassert>
#include <iostream>
#include <map>
#include <random>
#include <thread>
#include <unordered_map>

#include "b-tree.hpp"

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
  std::random_device rd;  // Используем для генерации начального значения
  std::mt19937 generator(rd());  // Генератор случайных чисел
  std::uniform_int_distribution<> distribution(
      1, 100);  // Распределение случайных чисел

  btree::BLinkTree<int, int, 2> t;

  auto v = t.GetValues();
  auto va = fff(a);

  // fill test
  for (int i = 90; i > 1; --i) {
    const auto x = distribution(generator);
    const auto y = distribution(generator);
    // std::cout << "id is "<< i << " current x y are: " << x << " and " << y <<
    // '\n';
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
  }
  std::cout << '\n';
  for (auto i : ta) {
    std::cout << i << ' ';
  }
  std::cout << '\n';

  std::cout << t.size() << ' ' << a.size() << '\n';
}

void SimpleTestPlusRm(void) {
  std::map<int, int> a;
  std::unordered_map<int, int> b;
  std::random_device rd;  // Используем для генерации начального значения
  std::mt19937 generator(rd());  // Генератор случайных чисел
  std::uniform_int_distribution<> distribution(
      1, 100);  // Распределение случайных чисел

  btree::BLinkTree<int, int, 2> t;

  auto v = t.GetValues();
  auto va = fff(a);

  // fill test
  for (int i = 20; i > 1; --i) {
    const auto x = distribution(generator);
    const auto y = distribution(generator);
    // std::cout << "id is "<< i << " current x y are: " << x << " and " << y <<
    // '\n';
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
  }
  std::cout << '\n';
  for (auto i : ta) {
    std::cout << i << ' ';
  }
  std::cout << '\n';

  std::cout << t.size() << ' ' << a.size() << '\n';

  for (const auto& [key, value] : b) {
    assert(t.contains(key));
    std::cout << key << '\t' << t.at(key) << '\n';
    assert(t.at(key) == value);
    t.erase(key);
    assert(!t.contains(key));
  }
}

int main() {
  SimpleTest();
  return 0;
}
