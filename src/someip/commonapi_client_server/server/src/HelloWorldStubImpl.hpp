#ifndef HELLOWORLDSTUBIMPL_HPP
#define HELLOWORLDSTUBIMPL_HPP

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/HelloWorldStubDefault.hpp>
#include <sstream>
#include <iostream>

class HelloWorldStubImpl : public v0_1::commonapi::HelloWorldStubDefault {
public:
    HelloWorldStubImpl();
    virtual ~HelloWorldStubImpl();

    virtual void sayHello(const std::shared_ptr<CommonAPI::ClientId> _client,
                          std::string _name,
                          sayHelloReply_t _reply);
};

#endif // HELLOWORLDSTUBIMPL_HPP

