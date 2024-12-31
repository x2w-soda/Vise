rem build Visual Studio Solution
git submodule init
git submodule update
rmdir /s /q Build
mkdir Build
cd Build
cmake ..
cmake --build .
start Vise.sln
