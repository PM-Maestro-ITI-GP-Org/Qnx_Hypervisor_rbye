#ifndef MOTORDATASTUBIMPL_HPP
#define MOTORDATASTUBIMPL_HPP

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/MotorDataServiceStubDefault.hpp>

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#define WINDOW_SIZE  200u

class MotorDataStubImpl : public v0_1::commonapi::MotorDataServiceStubDefault {
public:
    MotorDataStubImpl();
    virtual ~MotorDataStubImpl();

    virtual void sendBatch(const std::shared_ptr<CommonAPI::ClientId> _client,
                           uint64_t _timestamp,
                           uint32_t _producerSeq,
                           uint16_t _flags,
                           uint32_t _blockRows,
                           std::vector<v0_1::commonapi::MotorDataService::MotorRow> _rows,
                           sendBatchReply_t _reply);

private:
    std::ofstream csvFile_;
    std::mutex    csvMutex_;
    uint64_t      totalRowsWritten_{0};

    void ensureCsvHeader();

    std::string runAnomalyDetection(const std::vector<v0_1::commonapi::MotorDataService::MotorRow> &rows);
    std::string runFaultClassification(const std::vector<v0_1::commonapi::MotorDataService::MotorRow> &rows);
    std::string runPredictiveMaintenance(const std::vector<v0_1::commonapi::MotorDataService::MotorRow> &rows);
};

#endif
