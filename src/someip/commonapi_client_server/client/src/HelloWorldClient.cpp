#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/HelloWorldProxy.hpp>

using namespace v0_1::commonapi;

static std::atomic<bool> g_running{true};

static void signalHandler(int signum)
{
    g_running = false;
}

int main()
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    CommonAPI::Runtime::setProperty("LogContext", "HWLC");
    CommonAPI::Runtime::setProperty("LogApplication", "HWLC");
    CommonAPI::Runtime::setProperty("LibraryBase", "HelloWorld");

    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();

    std::string domain     = "local";
    std::string instance   = "commonapi.HelloWorld";
    std::string connection = "client-sample";

    std::shared_ptr<HelloWorldProxy<>> myProxy =
        runtime->buildProxy<HelloWorldProxy>(domain, instance, connection);

    std::cout << "Checking availability!" << std::endl;
    while (!myProxy->isAvailable() && g_running)
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    std::cout << "Available..." << std::endl;

    const std::string name = "World";
    CommonAPI::CallStatus callStatus;
    std::string returnMessage;

    while (g_running) {
        myProxy->sayHello(name, callStatus, returnMessage);
        if (callStatus != CommonAPI::CallStatus::SUCCESS) {
            std::cerr << "Remote call failed!\n";
            return -1;
        }
        std::cout << "Got message: '" << returnMessage << "'\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Client shutting down..." << std::endl;
    return 0;
}

