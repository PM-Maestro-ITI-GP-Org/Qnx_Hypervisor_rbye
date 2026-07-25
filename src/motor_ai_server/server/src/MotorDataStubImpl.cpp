#include "MotorDataStubImpl.hpp"

#include <cstdio>
#include <iostream>

static const char *CSV_NEW = "/tmp/motor_data_new.csv";
static const char *CSV_OLD = "/tmp/motor_data_old.csv";

MotorDataStubImpl::MotorDataStubImpl()
{
    csvFile_.open(CSV_NEW, std::ios::trunc);
    if (csvFile_.is_open()) {
        std::cout << "[AI-server] CSV file: " << CSV_NEW << " (window=" << WINDOW_SIZE << ")" << std::endl;
    } else {
        std::cerr << "[AI-server] ERROR: could not open CSV file: " << CSV_NEW << std::endl;
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

void MotorDataStubImpl::rotateCsv()
{
    csvFile_.close();
    /* current motor_data_new.csv is now a complete batch: move it to old */
    std::remove(CSV_OLD);
    std::rename(CSV_NEW, CSV_OLD);
    /* start a fresh new file for the next batch */
    csvFile_.open(CSV_NEW, std::ios::trunc);
    if (csvFile_.is_open()) {
        totalRowsWritten_ = 0;
        ensureCsvHeader();
        std::cout << "[AI-server] CSV rotated: " << CSV_NEW << " (batch saved to " << CSV_OLD << ")" << std::endl;
    } else {
        std::cerr << "[AI-server] ERROR: could not reopen CSV after rotation: " << CSV_NEW << std::endl;
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
            csvFile_ << r.getTimestamp() << ","
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
        if (totalRowsWritten_ >= WINDOW_SIZE) {
            rotateCsv();
        }
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
