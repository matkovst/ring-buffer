Магический круговой буфер (Ring buffer)
=======================================

Однофайловая реализация кругового буфера на C++ без сторонних зависимостей.
Два соседних виртуальных блока транслируются на общую физическую память, что позволяет поддерживать содержимое буфера в непрерывном и упорядоченном виде.

Требуется поддержка C++11 или выше.

Компиляция
==========

Компиляция юнит-тестов через gcc/clang

    clang++ -g -O2 -Wall -Wextra -Wno-unused-function ring_buffer_tests.cpp -o ring_buffer_tests

Компиляция юнит-тестов с визуальным выводом через gcc/clang

    clang++ -g -O2 -Wall -Wextra -Wno-unused-function -Dcimg_display=1 ring_buffer_tests.cpp -o ring_buffer_tests -lX11 -lpthread

Компиляция тестов производительности через gcc/clang

    clang++ -g -O2 ring_buffer_perftests.cpp -o ring_buffer_perftests

Кросс-компиляция юнит-тестов для другого ABI через zig

    zig c++ -target x86_64-linux-gnu.2.16 -Wall -Werror -Wfatal-errors ring_buffer_tests.cpp -o ring_buffer_tests_gnu.2.16

Профилирование
==============

Проверка на утечки памяти

    valgrind --leak-check=full --show-leak-kinds=all ./ring_buffer_tests

Профилирование по системных счетчикам

    perf stat --repeat=32 -dd ./ring_buffer_perftests ring 1

    Первый аргумент - тип теста: malloc, mmap, ring
    Второй аргумент - условный объем буфера, по возрастанию: 0, 1, 2