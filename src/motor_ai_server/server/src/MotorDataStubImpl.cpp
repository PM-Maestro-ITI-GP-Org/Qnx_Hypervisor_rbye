#include "MotorDataStubImpl.hpp"

#include <ctime>
#include <sstream>
#include <iostream>

static std::string timestampStr()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", std::gmtime(&t));
    return buf;
}

MotorDataStubImpl::MotorDataStubImpl()
{
    std::string path = "/tmp/motor_data_" + timestampStr() + ".csv";
    csvFile_.open(path, std::ios::app);
    if (csvFile_.is_open()) {
        std::cout << "[AI-server] CSV file: " << path << std::endl;
    } else {
        std::cerr << "[AI-server] ERROR: could not open CSV file: " << path << std::endl;
    }
}

MotorDataStubImpl::~MotorDataStubImpl()
{
    if (csvFile_.is_open()) csvFile_.close();
    std::cout << "[AI-server] total rows written: " << totalRowsWritten_ << std::endl;
}

void MotorDataStubImpl::ensureCsvHeader()
{
    if (totalRowsWritten_ == 0 && csvFile_.is_open()) {
        csvFile_ << "timestamp,producer_seq,flags,"
                 << "current_a,current_b,current_c,"
                 << "voltage_a,voltage_b,voltage_c,voltage_dc_bus,voltage_speed,"
                 << "vib_x,vib_y,vib_z,rpm"
                 << std::endl;
    }
}

std::string MotorDataStubImpl::runAnomalyDetection(
    const std::vector<v0_1::commonapi::MotorDataService::MotorRow> &rows)
{
    printf("[AI-model] runAnomalyDetection: %zu rows\n", rows.size());
    return "normal";
}

std::string MotorDataStubImpl::runFaultClassification(
    const std::vector<v0_1::commonapi::MotorDataService::MotorRow> &rows)
{
    printf("[AI-model] runFaultClassification: %zu rows\n", rows.size());
    return "none";
}

std::string MotorDataStubImpl::runPredictiveMaintenance(
    const std::vector<v0_1::commonapi::MotorDataService::MotorRow> &rows)
{
    printf("[AI-model] runPredictiveMaintenance: %zu rows\n", rows.size());
    return "RUL: N/A";
}

void MotorDataStubImpl::sendBatch(
    const std::shared_ptr<CommonAPI::ClientId> _client,
    uint64_t _timestamp,
    uint32_t _producerSeq,
    uint16_t _flags,
    uint32_t _blockRows,
    std::vector<v0_1::commonapi::MotorDataService::MotorRow> _rows,
    sendBatchReply_t _reply)
{
    (void)_client;

    {
        std::lock_guard<std::mutex> lock(csvMutex_);

        ensureCsvHeader();

        size_t n = _rows.size();
        for (size_t i = 0; i < n; ++i) {
            const auto &r = _rows[i];
            csvFile_ << _timestamp << ","
                     << _producerSeq << ","
                     << _flags << ","
                     << r.getCurrentA() << ","
                     << r.getCurrentB() << ","
                     << r.getCurrentC() << ","
                     << r.getVoltageA() << ","
                     << r.getVoltageB() << ","
                     << r.getVoltageC() << ","
                     << r.getVoltageDcBus() << ","
                     << r.getVoltageSpeed() << ","
                     << r.getVibX() << ","
                     << r.getVibY() << ","
                     << r.getVibZ() << ","
                     << r.getRpm()
                     << std::endl;
        }
        totalRowsWritten_ += n;
    }

    std::cout << "[AI-server] batch: " << _rows.size() << " rows (seq=" << _producerSeq
              << ", total=" << totalRowsWritten_ << ", window=" << WINDOW_SIZE << ")" << std::endl;

    std::string anomalyResult = runAnomalyDetection(_rows);

    std::string faultClassResult = "none";
    std::string predMaintResult  = "RUL: N/A";

    if (anomalyResult != "normal") {
        std::cout << "[AI-server] anomaly detected -> running fault classification & predictive maintenance"
                  << std::endl;
        faultClassResult = runFaultClassification(_rows);
        predMaintResult  = runPredictiveMaintenance(_rows);
    } else {
        std::cout << "[AI-server] no anomaly detected" << std::endl;
    }

    _reply(true, anomalyResult, faultClassResult, predMaintResult);
}
