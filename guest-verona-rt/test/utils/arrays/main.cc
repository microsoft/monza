// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <arrays.h>
#include <cstdio>
#include <test.h>

using namespace monza;

void test_unique()
{
  UniqueArray<int> empty{};

  test_check(empty.size() == 0);
  test_check(std::span(empty).empty());

  UniqueArray<int> initializer_list({1, 2, 3, 4});

  test_check(initializer_list.size() == 4);
  test_check(!std::span(initializer_list).empty());

  UniqueArray<int> initializer_list_assigned = {1, 2, 3, 4};

  test_check(initializer_list_assigned.size() == 4);
  test_check(!std::span(initializer_list_assigned).empty());

  UniqueArray<int> moved_constructor(std::move(initializer_list));
  test_check(moved_constructor.size() == 4);
  test_check(!std::span(moved_constructor).empty());
  test_check(initializer_list.size() == 0);
  test_check(std::span(initializer_list).empty());

  UniqueArray<int> moved_assignment{};
  moved_assignment = std::move(moved_constructor);
  test_check(moved_assignment.size() == 4);
  test_check(!std::span(moved_assignment).empty());
  test_check(moved_constructor.size() == 0);
  test_check(std::span(moved_constructor).empty());

  puts("SUCCESS: test_unique");
}

void test_shared()
{
  SharedArray<int> empty{};

  test_check(empty.size() == 0);
  test_check(std::span(empty).empty());

  SharedArray<int> initializer_list({1, 2, 3, 4});

  test_check(initializer_list.size() == 4);
  test_check(!std::span(initializer_list).empty());

  SharedArray<int> initializer_list_assigned = {1, 2, 3, 4};

  test_check(initializer_list_assigned.size() == 4);
  test_check(!std::span(initializer_list_assigned).empty());

  SharedArray<int> moved_constructor(std::move(initializer_list));
  test_check(moved_constructor.size() == 4);
  test_check(!std::span(moved_constructor).empty());
  test_check(initializer_list.size() == 0);
  test_check(std::span(initializer_list).empty());

  SharedArray<int> moved_assignment{};
  moved_assignment = std::move(moved_constructor);
  test_check(moved_assignment.size() == 4);
  test_check(!std::span(moved_assignment).empty());
  test_check(moved_constructor.size() == 0);
  test_check(std::span(moved_constructor).empty());

  SharedArray<int> copied_constructor(moved_assignment);
  test_check(copied_constructor.size() == 4);
  test_check(!std::span(copied_constructor).empty());
  test_check(moved_assignment.size() == 4);
  test_check(!std::span(moved_assignment).empty());

  SharedArray<int> copied_assignment{};
  copied_assignment = moved_assignment;
  test_check(copied_assignment.size() == 4);
  test_check(!std::span(copied_assignment).empty());
  test_check(moved_assignment.size() == 4);
  test_check(!std::span(moved_assignment).empty());

  puts("SUCCESS: test_shared");
}

int main()
{
  test_unique();
  test_shared();

  return 0;
}
