cmake_minimum_required(VERSION 3.20)
project(residual_compression LANGUAGES CXX)

add_library(residual_compression src/cpu_rice_golomb_codec.cpp src/codec_factory.cpp)
target_include_directories(residual_compression PUBLIC include)
target_compile_features(residual_compression PUBLIC cxx_std_20)
target_compile_options(residual_compression PRIVATE -Wall -Wextra -Wpedantic)
add_library(ResidualCompression::residual_compression ALIAS residual_compression)

if(BUILD_TESTING)
  add_executable(residual_compression_tests tests/residual_compression_tests.cpp)
  target_link_libraries(residual_compression_tests PRIVATE residual_compression)
  add_test(NAME residual_compression_tests COMMAND residual_compression_tests)
endif()
