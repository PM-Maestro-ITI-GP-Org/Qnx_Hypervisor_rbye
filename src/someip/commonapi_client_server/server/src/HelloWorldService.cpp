#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

#include <CommonAPI/CommonAPI.hpp>
#include "HelloWorldStubImpl.hpp"

static std::atomic<bool> g_running{true};

static void signalHandler(int signum)
{
    g_running = false;
}

int main()
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    CommonAPI::Runtime::setProperty("LogContext", "HWLS");
    CommonAPI::Runtime::setProperty("LogApplication", "HWLS");
    CommonAPI::Runtime::setProperty("LibraryBase", "HelloWorld");

    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();

    std::string domain    = "local";
    std::string instance  = "commonapi.HelloWorld";
    std::string connection = "service-sample";

    std::shared_ptr<HelloWorldStubImpl> myService =
        std::make_shared<HelloWorldStubImpl>();

    while (!runtime->registerService(domain, instance, myService, connection) && g_running) {
        std::cout << "Register Service failed, trying again in 100 ms..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!g_running) {
        std::cout << "Service shutting down..." << std::endl;
        return 0;
    }

    std::cout << "Successfully Registered Service!" << std::endl;

    while (g_running) {
        std::cout << "Waiting for calls... (Abort with CTRL+C)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    std::cout << "Service shutting down..." << std::endl;
    return 0;
}

