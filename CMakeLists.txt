cmake_minimum_required(VERSION 3.22)

project(InputMixer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Dependencies)
include(BuildRust)

add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)

juce_add_modules("${CMAKE_CURRENT_SOURCE_DIR}/external/fontaudio/wrappers/juce/module/fontaudio")

add_subdirectory(shell)
