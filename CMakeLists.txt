cmake_minimum_required(VERSION 3.16)
project(cpp_lru_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library: consumers just need the include directory.
add_library(lru_cache INTERFACE)
target_include_directories(lru_cache INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Opt-in ThreadSanitizer for the concurrency test:
#   cmake -B build -DENABLE_TSAN=ON
option(ENABLE_TSAN "Build with ThreadSanitizer (-fsanitize=thread)" OFF)

enable_testing()
find_package(Threads REQUIRED)

# Warnings applied to our test targets (not forced on library consumers).
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)

function(add_cache_test name)
  add_executable(${name} tests/${name}.cpp)
  target_link_libraries(${name} PRIVATE lru_cache project_warnings Threads::Threads)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  if(ENABLE_TSAN)
    target_compile_options(${name} PRIVATE -fsanitize=thread -g)
    target_link_options(${name} PRIVATE -fsanitize=thread)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_cache_test(test_lru_cache)
