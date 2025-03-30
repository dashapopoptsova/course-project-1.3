cmake_minimum_required(VERSION 3.10)
project(MyCppProject)

# Настройка стандарта C++ (например, C++17)
set(CMAKE_CXX_STANDARD 17)

# Добавляем исполняемый файл
add_executable(my_app src/main.cpp)

# Если есть заголовки
target_include_directories(my_app PRIVATE include)

# Подключение библиотек (например, через vcpkg)
find_package(xlnt REQUIRED)
target_link_libraries(my_app PRIVATE Boost::boost)