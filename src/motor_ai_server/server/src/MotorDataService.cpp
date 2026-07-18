#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

#include <CommonAPI/CommonAPI.hpp>
#include "MotorDataStubImpl.hpp"

static std::atomic<bool> g_running{true};

static void signalHandler(int signum)
{
    (void)signum;
    g_running = false;
}

int main()
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    CommonAPI::Runtime::setProperty("LogContext", "MDAI");
    CommonAPI::Runtime::setProperty("LogApplication", "MDAI");
    CommonAPI::Runtime::setProperty("LibraryBase", "MotorDataService");

    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();

    std::string domain    = "local";
    std::string instance  = "commonapi.MotorDataService";
    std::string connection = "motor-ai-service";

    std::shared_ptr<MotorDataStubImpl> myService =
        std::make_shared<MotorDataStubImpl>();

    while (!runtime->registerService(domain, instance, myService, connection) && g_running) {
        std::cout << "[AI-server] Register Service failed, retrying in 100ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!g_running) {
        std::cout << "[AI-server] shutting down before registration" << std::endl;
        return 0;
    }

    std::cout << "[AI-server] registered. Waiting for motor data batches..." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[AI-server] shutting down" << std::endl;
    return 0;
}
