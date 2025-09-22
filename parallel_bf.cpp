// Filename: parallel_bf.cpp
// Compile with: g++ -std=c++17 -O3 -o parallel_bf parallel_bf.cpp -pthread
#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <condition_variable>

// Configuration
struct Config {
    static constexpr const char* EXECUTABLE_PATH = "./build/5g-sniffer/5g-sniffer";
    static constexpr const char* IQ_FILE_PATH = "/home/avi/cwireless/5g-sniffer/data/gnb_ue_tr.fc32";
    
    static constexpr int MIN_PRB = 29;
    static constexpr int MAX_PRB = 107;
    static constexpr int MIN_OFFSET = 0;
    static constexpr int MAX_OFFSET = 500;
    static constexpr int OFFSET_STEP = 1;
    
    static constexpr int TIMEOUT_SECONDS = 15;
};

// Globals
std::atomic<int> completed{0};
std::atomic<int> total{0};
std::atomic<bool> found{false};
std::mutex print_mutex;

struct Task {
    int prb;
    int offset;
    int id;
};

class SimpleQueue {
    std::queue<Task> q;
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    
public:
    void push(Task t) {
        {
            std::lock_guard<std::mutex> lock(m);
            q.push(t);
        }
        cv.notify_one();
    }
    
    bool pop(Task& t) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this]{ return !q.empty() || done; });
        if (q.empty()) return false;
        t = q.front();
        q.pop();
        return true;
    }
    
    void finish() {
        {
            std::lock_guard<std::mutex> lock(m);
            done = true;
        }
        cv.notify_all();
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(m);
        return q.size();
    }
};

// command execution
std::string exec_cmd(const std::string& cmd) {
    char buffer[512];
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
        if (result.find("Found DCI PDCCH DCI") != std::string::npos) {
            pclose(pipe);
            return result;
        }
    }
    pclose(pipe);
    return result;
}

void save_debug_output(int task_id, int prb, int offset, const std::string& output) {
    std::string filename = "/tmp/sniffer_task_" + std::to_string(task_id) + ".txt";
    std::ofstream f(filename);
    if (f.is_open()) {
        f << "Task ID: " << task_id << "\n";
        f << "PRB: " << prb << ", Offset: " << offset << "\n";
        f << "Output Length: " << output.size() << " bytes\n";
        f << "--- Output ---\n" << output << "\n";
    }
}

void worker(SimpleQueue& queue, int thread_id) {
    Task task;
    
    while (queue.pop(task) && !found.load()) {
        char config_file[256];
        snprintf(config_file, sizeof(config_file), "/tmp/cfg_%d_%d.toml", thread_id, task.id);
        
        int port = 23500 + thread_id;
        
        // --- THE FIX IS HERE ---
        // Use C++ string streams to safely and correctly build the config content.
        std::ostringstream config_stream;
        config_stream << "[sniffer]\n"
                      << "file_path = \"" << Config::IQ_FILE_PATH << "\"\n"
                      << "sample_rate = 23040000\n"
                      << "frequency = 1842500000\n"
                      << "nid_1 = 1\n"
                      << "ssb_numerology = 0\n"
                      << "zmq_port = " << port << "\n\n"
                      << "[[pdcch]]\n"
                      << "coreset_id = 1\n"
                      << "subcarrier_offset = " << task.offset << "\n"
                      << "num_prbs = " << task.prb << "\n"
                      << "numerology = 0\n"
                      << "dci_sizes_list = [41]\n"
                      << "scrambling_id_start = 1\n"
                      << "scrambling_id_end = 10\n"
                      << "rnti_start = 17921\n"
                      << "rnti_end = 17930\n"
                      << "interleaving_pattern = \"non-interleaved\"\n"
                      << "coreset_duration = 1\n"
                      << "AL_corr_thresholds = [1.0, 0.5, 0.5, 1.0, 1.0]\n"
                      << "num_candidates_per_AL = [0, 4, 4, 0, 0]\n";

        {
            std::ofstream cfg(config_file);
            cfg << config_stream.str();
        }
        
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "timeout %d %s %s 2>&1", 
                 Config::TIMEOUT_SECONDS, Config::EXECUTABLE_PATH, config_file);
        
        std::string output = exec_cmd(cmd);
        
        if (task.id <= 10 || task.id % 1000 == 0) {
            save_debug_output(task.id, task.prb, task.offset, output);
            
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "\n[DEBUG] Saved output for task " << task.id 
                      << " (PRB=" << task.prb << ", Offset=" << task.offset 
                      << ") to /tmp/sniffer_task_" << task.id << ".txt\n" << std::flush;
        }
        
        if (output.find("Found DCI") != std::string::npos || 
            output.find("RNTI = 17921") != std::string::npos) {
            
            found.store(true);
            
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "\n\n======================================\n";
            std::cout << "SUCCESS! Found signal!\n";
            std::cout << "PRB: " << task.prb << "\n";
            std::cout << "Offset: " << task.offset << "\n";
            std::cout << "Task ID: " << task.id << "\n";
            std::cout << "======================================\n";
            std::cout << "Output:\n" << output.substr(0, 1000) << "\n";
            std::cout << "======================================\n\n";
            
            std::ofstream success_file("/tmp/SUCCESS_PARAMS.txt");
            success_file << "PRB=" << task.prb << " OFFSET=" << task.offset << "\n";
            success_file << output;
            success_file.close();
            
            std::cout << "Parameters saved to /tmp/SUCCESS_PARAMS.txt\n" << std::flush;
        }
        
        std::remove(config_file);
        
        completed++;
        
        if (completed % 10 == 0) {
            int pct = (completed * 100) / total;
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "\rProgress: " << pct << "% (" << completed.load() 
                      << "/" << total << ") | Current: PRB=" << task.prb 
                      << ", Offset=" << task.offset << " | Queue: " << queue.size() 
                      << "        " << std::flush;
        }
    }
}

int main() {
    if (!std::filesystem::exists(Config::EXECUTABLE_PATH)) {
        std::cerr << "ERROR: Sniffer executable not found at: " << Config::EXECUTABLE_PATH << "\n";
        return 1;
    }
    if (!std::filesystem::exists(Config::IQ_FILE_PATH)) {
        std::cerr << "ERROR: IQ file not found at: " << Config::IQ_FILE_PATH << "\n";
        return 1;
    }
    
    std::cout << "\n=== Simple Parallel 5G Scanner ===\n";
    std::cout << "Executable: " << Config::EXECUTABLE_PATH << "\n";
    std::cout << "IQ File: " << Config::IQ_FILE_PATH << "\n";
    std::cout << "PRB Range: " << Config::MIN_PRB << " to " << Config::MAX_PRB << "\n";
    std::cout << "Offset Range: " << Config::MIN_OFFSET << " to " << Config::MAX_OFFSET 
              << " (step " << Config::OFFSET_STEP << ")\n";
    
    std::system("rm -f /tmp/cfg_*.toml /tmp/sniffer_task_*.txt /tmp/SUCCESS_PARAMS.txt 2>/dev/null");
    
    SimpleQueue queue;
    int task_id = 1;
    
    std::vector<Task> all_tasks;
    for (int prb = Config::MIN_PRB; prb <= Config::MAX_PRB; prb++) {
        for (int offset = Config::MIN_OFFSET; offset <= Config::MAX_OFFSET; offset += Config::OFFSET_STEP) {
            all_tasks.push_back({prb, offset, task_id++});
        }
    }
    
    total = all_tasks.size();
    std::cout << "Total tasks: " << total << "\n";
    
    int num_threads = std::min(11, (int)std::thread::hardware_concurrency());

    std::cout << "Using " << num_threads << " threads\n";
    std::cout << "\nStarting scan...\n\n";
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; i++) {
        workers.emplace_back(worker, std::ref(queue), i);
    }
    
    for (const auto& task : all_tasks) {
        if (found.load()) break;
        queue.push(task);
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // Don't flood
    }
    
    queue.finish();
    
    for (auto& w : workers) {
        w.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    std::cout << "\n\n=== Scan Complete ===\n";
    std::cout << "Completed: " << completed.load() << "/" << total << " tasks\n";
    std::cout << "Time: " << duration.count() << " seconds\n";
    std::cout << "Rate: " << (completed.load() / (duration.count() + 1)) << " tasks/second\n";
    
    if (found.load()) {
        std::cout << "\nSUCCESS! Check /tmp/SUCCESS_PARAMS.txt for the winning parameters.\n";
    } else {
        std::cout << "\nNo signal found in the search range.\n";
        std::cout << "Check /tmp/sniffer_task_*.txt files for debug output.\n";
    }
    
    return 0;
}


