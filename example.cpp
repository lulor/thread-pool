#include <iostream>

#include "thread_pool.hpp"

enum command { submit1, submit2, result, status, help, quit, terminate, unknown };

auto getCommand(const std::string &s) -> command {
    if (s == "1") return submit1;
    if (s == "2") return submit2;
    if (s == "r") return result;
    if (s == "p") return status;
    if (s == "h") return help;
    if (s == "q") return quit;
    if (s == "t") return terminate;
    return unknown;
}

void printHelp(int n1, int n2) {
    std::cout << "1: submit " << n1 << " times the task1 (random number)" << std::endl;
    std::cout << "2: submit " << n2 << " times the task2 (random string)" << std::endl;
    std::cout << "r: retrieve all the results" << std::endl;
    std::cout << "h: print this help message" << std::endl;
    std::cout << "q: terminate the thread-pool and quit" << std::endl;
    std::cout << "p: show the thread-pool status" << std::endl;
    std::cout << "t: terminate the thread-pool" << std::endl;
}

auto generateString(int min_len, int max_len) -> std::string {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char buf[max_len];
    int len = (rand() % (max_len - min_len)) + min_len;
    for (int i = 0; i < len; i++) buf[i] = charset[rand() % (sizeof(charset) - 1)];
    buf[len] = 0;
    return buf;
};

auto submitTask1(ThreadPool &tp) -> std::future<int> {
    auto task = [](int range) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return rand() % range;
    };
    return tp.submit(task, INT_MAX);
}

auto submitTask2(ThreadPool &tp) -> std::future<std::string> {
    auto task = [](int min_len, int max_len) {
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        return generateString(min_len, max_len);
    };
    return tp.submit(task, 30, 80);
}

void menu(ThreadPool &tp) {
    bool stop = false;
    int n1 = 50;
    int n2 = 30;
    std::vector<std::future<int>> futures1;
    std::vector<std::future<std::string>> futures2;
    futures1.reserve(n1);
    futures2.reserve(n2);

    std::cout << "=== MENU ===" << std::endl;
    printHelp(n1, n2);

    while (!stop) {
        std::string line;
        std::cout << "command: ";
        std::getline(std::cin, line);

        command c = getCommand(line);

        switch (c) {
            case quit:
                stop = true;
                break;
            case submit1:
                futures1.clear();
                for (int i = 0; i < n1; i++) futures1.push_back(submitTask1(tp));
                break;
            case submit2:
                futures2.clear();
                for (int i = 0; i < n2; i++) futures2.push_back(submitTask2(tp));
                break;
            case result:
                for (int i = 0; i < futures1.size(); i++)
                    std::cout << "Task1 " << i << " : " << futures1[i].get() << std::endl;
                for (int i = 0; i < futures2.size(); i++)
                    std::cout << "Task2 " << i << " : " << futures2[i].get() << std::endl;
                /* the futures must be deleted since they are "consumed" */
                futures1.clear();
                futures2.clear();
                break;
            case status:
                tp.printStatus();
                break;
            case help:
                printHelp(n1, n2);
                break;
            case terminate:
                tp.terminate();
                break;
            default:
                std::cout << "Unknown command" << std::endl;
                break;
        }
    }
}

int func(int a) { return a * 4; }

auto main() -> int {
    ThreadPool tp(4, 8, 100);
    try {
        {
            std::vector<int> v = {10, 9, 23, 4, 0};
            auto fn = [&v]() { std::sort(v.begin(), v.end()); };
            auto f = tp.submit(fn);
            f.get();
            for (auto val : v) std::cout << val << " ";
            std::cout << std::endl;
        }
        {
            auto f = tp.submit([](const std::string &str) { std::cout << str << std::endl; }, "ciao");
            f.get();
        }
        menu(tp);
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}