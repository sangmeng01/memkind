// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (C) 2020 - 2021 Intel Corporation. */

#include <memkind.h>
#include <numa.h>
#include <numaif.h>
#include <math.h>
#include <string>
#include <unistd.h>

#include "proc_stat.h"
#include "sys/types.h"
#include "sys/sysinfo.h"
#include "TestPolicy.hpp"

#include "common.h"

class MemkindHiCapacityFunctionalTests:
    public ::testing::Test
{
protected:

    void SetUp() {}
    void TearDown() {}
};

class MemkindHiCapacityFunctionalTestsParam: public ::testing::Test,
    public ::testing::WithParamInterface<memkind_t>
{
protected:
    memkind_t memory_kind;

    void SetUp()
    {
        memory_kind = GetParam();
    }
    void TearDown() {}
};

INSTANTIATE_TEST_CASE_P(
    KindParam, MemkindHiCapacityFunctionalTestsParam,
    ::testing::Values(MEMKIND_HIGHEST_CAPACITY,
                      MEMKIND_HIGHEST_CAPACITY_PREFERRED));

TEST_P(MemkindHiCapacityFunctionalTestsParam,
       test_TC_HiCapacity_alloc_size_max)
{
    errno = 0;
    void *test1 = memkind_malloc(memory_kind, SIZE_MAX);
    ASSERT_EQ(test1, nullptr);
    ASSERT_EQ(errno, ENOMEM);
}

// TODO move HWLOC check to some API and call it inside this function
#ifdef MEMKIND_HWLOC
TEST_F(MemkindHiCapacityFunctionalTests,
       test_TC_HiCapacityLocal_alloc_size_max)
{
    errno = 0;
    void *test1 = memkind_malloc(MEMKIND_HIGHEST_CAPACITY_LOCAL, SIZE_MAX);
    ASSERT_EQ(test1, nullptr);
    ASSERT_EQ(errno, ENOMEM);
}
#endif

TEST_P(MemkindHiCapacityFunctionalTestsParam, test_TC_HiCapacity_correct_numa)
{
    std::set<int> high_capacity_nodes = TestPolicy::get_highest_capacity_nodes();

    // TODO add API to check this in as general condition
    if (memory_kind == MEMKIND_HIGHEST_CAPACITY_PREFERRED &&
        high_capacity_nodes.size() != 1) {
        GTEST_SKIP() <<
                     "This test requires exactly 1 highest capacity NUMA Node in the OS.";
    }

    int size = 10;
    void *ptr = memkind_malloc(memory_kind, size);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0, size);

    // get ID and capacity of NUMA node where the allocation is made
    int numa_id = -1;
    int ret = get_mempolicy(&numa_id, nullptr, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR);
    ASSERT_EQ(ret, 0);
    long long numa_capacity = numa_node_size64(numa_id, NULL);

    // get capacity of NUMA node(s) that has the highest capacity in the system
    for(auto const &node: high_capacity_nodes) {
        long long capacity = numa_node_size64(node, NULL);
        ASSERT_EQ(numa_capacity, capacity);
    }

    memkind_free(memory_kind, ptr);
}

TEST_F(MemkindHiCapacityFunctionalTests,
       test_TC_HiCapacityPreferred_TwoOrMoreNodes)
{
    std::set<int> high_capacity_nodes = TestPolicy::get_highest_capacity_nodes();
    if (high_capacity_nodes.size() < 2) {
        GTEST_SKIP() <<
                     "This test requires minimum 2 highest capacity NUMA Nodes in the OS.";
    }
    const size_t alloc_size = 512;
    void *test1 = memkind_malloc(MEMKIND_HIGHEST_CAPACITY_PREFERRED, alloc_size);
    ASSERT_EQ(test1, nullptr);
}

TEST_P(MemkindHiCapacityFunctionalTestsParam,
       test_TC_HiCapacity_alloc_until_full_numa)
{
    std::set<int> high_capacity_nodes = TestPolicy::get_highest_capacity_nodes();

    // TODO add API to check this in as general condition
    if (memory_kind == MEMKIND_HIGHEST_CAPACITY_PREFERRED &&
        high_capacity_nodes.size() != 1) {
        GTEST_SKIP() <<
                     "This test requires exactly 1 highest capacity NUMA Node in the OS.";
    }
    ProcStat stat;
    void *ptr;
    const size_t alloc_size = 100 * MB;
    const size_t alloc_size_swap = 1 * MB;

    std::vector<void *> allocations;

    size_t sum_of_free_space = TestPolicy::get_free_space(high_capacity_nodes);
    int numa_id = -1;
    const int n_swap_alloc = 20;
    size_t sum_of_alloc = 0;
    while (sum_of_free_space > sum_of_alloc) {
        ptr = memkind_malloc(memory_kind, alloc_size);
        ASSERT_NE(nullptr, ptr);
        memset(ptr, 'a', alloc_size);
        allocations.emplace_back(ptr);
        sum_of_alloc += alloc_size;

        int ret = get_mempolicy(&numa_id, nullptr, 0, ptr, MPOL_F_NODE | MPOL_F_ADDR);
        ASSERT_EQ(ret, 0);

        // signal the moment when we actually reach free space of High Capacity Nodes
        if (high_capacity_nodes.find(numa_id) == high_capacity_nodes.end()) {
            ASSERT_GE(sum_of_alloc, 0.99*sum_of_free_space);
        }
    }

    size_t init_swap = stat.get_used_swap_space_size_bytes();
    for (int i = 0; i < n_swap_alloc; ++i) {
        ptr = memkind_malloc(memory_kind, alloc_size_swap);
        ASSERT_NE(nullptr, ptr);
        memset(ptr, 'a', alloc_size_swap);
        allocations.emplace_back(ptr);
    }

    // TODO add API to verify if variant is preferred
    if (memory_kind == MEMKIND_HIGHEST_CAPACITY_PREFERRED) {
        ASSERT_GE(stat.get_used_swap_space_size_bytes(), init_swap);
    } else {
        ASSERT_LE(stat.get_used_swap_space_size_bytes(), init_swap);
    }
    for (auto const &ptr: allocations) {
        memkind_free(memory_kind, ptr);
    }
}
