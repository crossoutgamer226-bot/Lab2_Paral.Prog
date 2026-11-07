#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <sstream>
#include <chrono> // Подключено для использования задержек (std::this_thread::sleep_for)

// Размер пачки для пакетной записи в файл
const size_t BATCH_SIZE = 5;

// Структура для хранения результата разложения
struct FactorResult {
    long long number;
    std::vector<long long> factors;
};

// Общие ресурсы с защитой мьютексами
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

// Функция, выполняемая каждым равноправным потоком (узлом)
void worker_node(int thread_id) {
    std::vector<FactorResult> local_buffer;
    local_buffer.reserve(BATCH_SIZE);

    while (true) {
        long long current_task = -1;

        // Критическая секция: захват задачи из общей очереди
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (task_queue.empty()) {
                break; // Задачи закончились
            }
            current_task = task_queue.front();
            task_queue.pop();
        }

        // МОДЕЛИРОВАНИЕ РАЗНОЙ СКОРОСТИ ИСПОЛНИТЕЛЕЙ (Дополнение по п. 3 задания):
        // Моделируем «тяжелую» задачу или «медленный» поток:
        // Если поток с ID 0 обрабатывает большое число, добавим искусственную задержку,
        // чтобы показать дисбаланс производительности, описанный в отчете.
        if (thread_id == 0 && current_task > 100000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Имитация медленной работы
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));  // Стандартная небольшая задержка
        }

        // Выполнение трудоемкой независимой задачи
        std::vector<long long> result_factors = factorize(current_task);

        // Сохранение результата в локальный буфер
        local_buffer.push_back({ current_task, result_factors });

        // Если буфер заполнился, производим пакетную запись в файл
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

    // Сброс оставшихся результатов из буфера по завершении работы потока
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
    // Установка локали для корректного ввода/вывода
    std::setlocale(LC_ALL, "Russian");

    std::string input_filename = "input.txt";
    std::string output_filename = "output.txt";

    // 1. Чтение входных данных из файла
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

    // 2. Открытие общего файла результатов
    output_file.open(output_filename);
    if (!output_file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл для записи " << output_filename << "\n";
        return 1;
    }

    // Определение количества доступных потоков (узлов)
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // резервное значение

    std::cout << "Запуск вычислений в " << num_threads << " равноправных потоках...\n";

    // 3. Создание пула равноправных потоков (рп-модель) с передачей ID потока
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_node, i);
    }

    // 4. Ожидание завершения работы всех потоков
    for (auto& th : threads) {
        th.join();
    }

    output_file.close();
    std::cout << "Вычисления завершены. Результаты записаны в файл " << output_filename << "\n";

    return 0;
}