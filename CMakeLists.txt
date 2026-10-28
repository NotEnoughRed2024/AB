cmake_minimum_required(VERSION 3.16)
project(Alphabounce CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# SDL3: use an installed copy if available, otherwise fetch it from GitHub.
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
  include(FetchContent)
  message(STATUS "SDL3 not found - fetching release-3.4.18 from GitHub")
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL3)
endif()

file(GLOB FLASH_SRC src/flash/*.cpp)
file(GLOB_RECURSE GAME_SRC src/game/*.cpp src/mt/*.cpp)

add_library(stb STATIC src/third_party_impl.c)
target_include_directories(stb PUBLIC third_party)

add_executable(alphabounce src/main.cpp ${FLASH_SRC} ${GAME_SRC})
target_include_directories(alphabounce PRIVATE src src/flash third_party)
target_link_libraries(alphabounce PRIVATE SDL3::SDL3 stb)

add_executable(symbolviewer tools/symbolviewer.cpp ${FLASH_SRC})
target_include_directories(symbolviewer PRIVATE src src/flash third_party)
target_link_libraries(symbolviewer PRIVATE SDL3::SDL3 stb)

if(MSVC)
  target_compile_options(alphabounce PRIVATE /W3)
else()
  target_compile_options(alphabounce PRIVATE -Wall -Wno-sign-compare -Wno-unused-variable)
endif()
