// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE.md file in the root directory of this source tree.

#include "concurrent_vector_test_common_types.h"

using TestTraitsTypes = ::testing::Types<TestTraitsA>;
DISPENSO_DISABLE_WARNING_PUSH
DISPENSO_DISABLE_WARNING_ZERO_VARIADIC_MACRO_ARGUMENTS
TYPED_TEST_SUITE(ConcurrentVectorTest, TestTraitsTypes);
DISPENSO_DISABLE_WARNING_POP

#include "concurrent_vector_test_common.h"
