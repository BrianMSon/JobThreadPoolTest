#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

const char* g_outLogFileName = "out.log";

// Job 구조체 정의
struct Job
{
    std::string _data;
};

class JobThreadPool
{
public:
    explicit JobThreadPool(int num_threads)
        : _stop(false)
        , _max_thread_count(num_threads)
    {
        for (int i = 0; i < num_threads; ++i)
        {
            _job_queues.emplace_back();
            _queue_mutexes.push_back(std::make_unique<std::mutex>());
            _workers.emplace_back([this, i]
            {
                while (true)
                {
                    // 해당 쓰레드의 큐와 뮤텍스 사용
                    std::unique_lock<std::mutex> lock(*_queue_mutexes[i]);
                    _condition.wait(lock, [this, i] { return _stop == true || !_job_queues[i].empty(); });
                    if (_stop == true && _job_queues[i].empty())
                        return;
                    Job job = std::move(_job_queues[i].front());
                    _job_queues[i].pop();
                    lock.unlock();

                    // Job 처리
                    processJob(job, i);
                }
            });
        }
    }

    // Job 추가
    void pushJob(Job job, int thread_id)
    {
        thread_id = thread_id % _max_thread_count;

        {
            std::lock_guard<std::mutex> lock(*_queue_mutexes[thread_id]);
            _job_queues[thread_id].push(std::move(job));
        }
        _condition.notify_one();
    }

    // 쓰레드풀 종료
    virtual ~JobThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(*_queue_mutexes[0]); // 아무 큐나 선택해서 종료
            _stop = true;
        }
        _condition.notify_all();
        for (auto& worker : _workers)
        {
            worker.join();
        }
    }

private:
    // Job 처리 함수
    void processJob(const Job& job, int thread_id)
    {
        std::lock_guard<std::mutex> lock(_file_mutex);

        printf("Thread %d : %s\n", thread_id, job._data.c_str());

        // 파일에 출력
        std::ofstream outfile(g_outLogFileName, std::ios_base::app);
        if (outfile.is_open())
        {
            outfile << "Thread " << thread_id << ": " << job._data << "\n";
            outfile.close();
        }
        else
        {
            std::cerr << "Error: Unable to open out.log for writing" << "\n";
        }
    }

private:
    std::vector<std::thread> _workers;
    std::vector<std::queue<Job>> _job_queues;
    std::vector<std::unique_ptr<std::mutex>> _queue_mutexes;

    std::mutex _file_mutex;
    std::condition_variable _condition;
    bool _stop;
    int _max_thread_count;
};

void initOutLogFile()
{
    // 파일을 새로 생성(초기화)
    std::ofstream outfile(g_outLogFileName);
    outfile.close();
}

int main()
{
    initOutLogFile();

    // JobThreadPool의 쓰레드 수
    const int kNumThreads = 10;
    JobThreadPool pool(kNumThreads);

    const int kLoopCount = 100;

    // 각 쓰레드에서 Job 생성하여 쓰레드풀에 추가
    std::thread t1([&pool]()
    {
        const int thread_id = 0;
        printf("[thread_id : %d]\n", thread_id);
        for (int i = 1; i <= kLoopCount; ++i)
        {
            pool.pushJob(Job{ "A-" + std::to_string(i) }, thread_id);
        }
    });
    std::thread t2([&pool]()
    {
        const int thread_id = 1;
        printf("[thread_id : %d]\n", thread_id);
        for (int i = 1; i <= kLoopCount; ++i)
        {
            pool.pushJob(Job{ "B-" + std::to_string(i) }, thread_id);
        }
    });
    std::thread t3([&pool]()
    {
        const int thread_id = 2;
        printf("[thread_id : %d]\n", thread_id);
        for (int i = 1; i <= kLoopCount; ++i)
        {
            pool.pushJob(Job{ "C-" + std::to_string(i) }, thread_id);
        }
    });

    std::cout << "main end.\n";

    t1.join();
    t2.join();
    t3.join();

    // 메인 쓰레드가 끝날 때까지 대기
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
