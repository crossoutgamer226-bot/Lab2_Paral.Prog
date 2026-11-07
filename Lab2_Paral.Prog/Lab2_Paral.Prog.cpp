#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <sstream>
#include <chrono>

// Размер пачки для пакетной записи в файл
const size_t BATCH_SIZE = 5;

// Структура для хранения результата разложения
struct FactorResult {
    long long number;
    std::vector<long long> factors;
};

// Общие ресурсы и примитивы синхронизации
std::mutex queue_mutex;
std::queue<long long> task_queue;
std::mutex file_mutex;
std::ofstream output_file;

// Функция разложения числа на простые множители
std::vector<long long> factorize(long long n) {
    std::vector<long long> factors;
    if (n <= 1) return factors;
    while (n % 2 == 0) {
        factors.push_back(2);
        n /= 2;
    }
    for (long long i = 3; i * i <= n; i += 2) {
        while (n % i == 0) {
            factors.push_back(i);
            n /= i;
        }
    }
    if (n > 1) {
        factors.push_back(n);
    }
    return factors;
}

// Функция рабочий-узел (рп-модель с равноправными узлами)
void worker_node(int thread_id) {
    std::vector<FactorResult> local_buffer;
    local_buffer.reserve(BATCH_SIZE);

    while (true) {
        long long current_task = -1;

        // Извлечение задачи из общей очереди
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (task_queue.empty()) {
                break;
            }
            current_task = task_queue.front();
            task_queue.pop();
        }

        // Моделирование разной скорости исполнителей (дисбаланса)
        if (thread_id == 0 && current_task > 100000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        // Выполнение задачи
        std::vector<long long> result_factors = factorize(current_task);
        local_buffer.push_back({ current_task, result_factors });

        // Пакетная запись при заполнении локального буфера
        if (local_buffer.size() >= BATCH_SIZE) {
            std::lock_guard<std::mutex> lock(file_mutex);
            for (const auto& res : local_buffer) {
                output_file << "Число " << res.number << ": ";
                for (size_t i = 0; i < res.factors.size(); ++i) {
                    output_file << res.factors[i] << (i + 1 < res.factors.size() ? " * " : "");
                }
                output_file << "\n";
            }
            output_file.flush();
            local_buffer.clear();
        }
    }

    // Сброс оставшихся результатов из буфера по завершении работы
    if (!local_buffer.empty()) {
        std::lock_guard<std::mutex> lock(file_mutex);
        for (const auto& res : local_buffer) {
            output_file << "Число " << res.number << ": ";
            for (size_t i = 0; i < res.factors.size(); ++i) {
                output_file << res.factors[i] << (i + 1 < res.factors.size() ? " * " : "");
            }
            output_file << "\n";
        }
        output_file.flush();
    }
}

int main() {
    std::setlocale(LC_ALL, "Russian");

    std::string input_filename = "input.txt";
    std::string output_filename = "output.txt";

    // Чтение входных данных
    std::ifstream input_file(input_filename);
    if (!input_file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << input_filename << "\n";
        return 1;
    }

    long long number;
    while (input_file >> number) {
        task_queue.push(number);
    }
    input_file.close();

    if (task_queue.empty()) {
        std::cout << "Файл входных данных пуст.\n";
        return 0;
    }

    // Открытие файла результатов
    output_file.open(output_filename);
    if (!output_file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл для записи " << output_filename << "\n";
        return 1;
    }

    // Инициализация и запуск пула равноправных потоков
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Запуск вычислений в " << num_threads << " равноправных потоках...\n";

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_node, i);
    }

    // Ожидание завершения работы всех потоков
    for (auto& th : threads) {
        th.join();
    }

    output_file.close();
    std::cout << "Вычисления завершены. Результаты записаны в файл " << output_filename << "\n";

    return 0;
}