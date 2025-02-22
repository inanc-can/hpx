# Copyright (c) 2023 The STE||AR-Group
#
# SPDX-License-Identifier: BSL-1.0
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(tests)

if(HPX_WITH_CXX20_COROUTINES)
  set(tests ${tests} generator)
endif()

foreach(test ${tests})

  set(test_PARAMETERS THREADS_PER_LOCALITY 4)

  set(sources ${test}.cpp)

  source_group("Source Files" FILES ${sources})

  add_hpx_executable(
    ${test}_test INTERNAL_FLAGS
    SOURCES ${sources} ${${test}_FLAGS}
    EXCLUDE_FROM_ALL
    HPX_PREFIX ${HPX_BUILD_PREFIX}
    FOLDER "Tests/Unit/Modules/Core/TypeSupport"
  )

  add_hpx_unit_test("modules.type_support" ${test} ${${test}_PARAMETERS})

endforeach()
