@echo off
REM Builds minigit.exe with MinGW g++ (works with old versions, C++11 only).
if not exist build mkdir build
g++ -std=c++11 -O2 -Wall -Iinclude ^
  src\main.cpp src\Commands.cpp src\Repository.cpp src\ObjectStore.cpp src\Objects.cpp ^
  src\Refs.cpp src\Index.cpp src\Undo.cpp src\Diff.cpp src\Trie.cpp src\Zlib.cpp ^
  src\Sha1.cpp src\FileUtil.cpp src\Common.cpp src\Server.cpp src\UiPage.cpp ^
  -o minigit.exe -lws2_32
if errorlevel 1 (echo Build FAILED) else (echo Built minigit.exe)
