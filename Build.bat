rem build Visual Studio Solution
rmdir /s /q Build
mkdir Build
cd Build
cmake ..
cmake --build .
start Vise.sln
