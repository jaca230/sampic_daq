cmake_minimum_required(VERSION 3.20)
project(template_fitter LANGUAGES CXX)

add_library(template_fitter
  src/cpu_least_squares_fitter.cpp
  src/cubic_pulse_template.cpp
  src/fitter_factory.cpp)
target_include_directories(template_fitter PUBLIC include)
target_compile_features(template_fitter PUBLIC cxx_std_20)
target_compile_options(template_fitter PRIVATE -Wall -Wextra -Wpedantic)
add_library(TemplateFitter::template_fitter ALIAS template_fitter)

if(BUILD_TESTING)
  add_executable(template_fitter_tests tests/template_fitter_tests.cpp)
  target_link_libraries(template_fitter_tests PRIVATE template_fitter)
  add_test(NAME template_fitter_tests COMMAND template_fitter_tests)
endif()
