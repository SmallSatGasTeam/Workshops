// ======================================================================
// \title  IMUWorkshop.cpp
// \author xtilloo
// \brief  cpp file for IMUWorkshop component implementation class
// ======================================================================

#include "Workshops/Components/IMUWorkshop/IMUWorkshop.hpp"
#include <Fw/Time/TimeInterval.hpp>
#include <Os/Task.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

IMUWorkshop ::IMUWorkshop(const char* const compName) : IMUWorkshopComponentBase(compName) {}

IMUWorkshop ::~IMUWorkshop() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

bool IMUWorkshop ::config() {
    U8 oprVal = 0;
    U8 pwrVal = 0;
    Fw::String msg;

    // Ensure register map Page 0 is selected before accessing Page 0 registers
    U8 pageVal = 0;
    if (!this->readConfigReg(this->PAGE_ID, pageVal, 0xFE)) {
        msg = "Failed to read PAGE_ID register.";
        this->log_WARNING_HI_configError(msg);
        this->configFails++;
        return false;
    }
    if (pageVal != 0x00 && !this->writeConfigReg(this->PAGE_ID, 0x00)) {
        msg = "Failed to select IMU register Page 0.";
        this->log_WARNING_HI_configError(msg);
        this->configFails++;
        return false;
    }

    // Read current operation mode (bits 7:4 are reserved 0 on BNO055)
    if (!this->readConfigReg(this->OPR_MODE, oprVal, 0xF0)) {
        msg = "Failed to read OPR_MODE register.";
        this->log_WARNING_HI_configError(msg);
        this->configFails++;
        return false;
    }
    oprVal &= 0x0F;

    // Read current power mode (bits 7:2 are reserved 0 on BNO055)
    if (!this->readConfigReg(this->PWR_MODE, pwrVal, 0xFC)) {
        msg = "Failed to read PWR_MODE register.";
        this->log_WARNING_HI_configError(msg);
        this->configFails++;
        return false;
    }
    pwrVal &= 0x03;

    // Only reconfigure registers if power mode is not NORMAL or operation mode is not NDOF
    if (pwrVal != this->PWR_NORMAL || oprVal != this->NDOF) {
        this->initialized = false;

        // BNO055 requires CONFIGMODE (0x00) before modifying configuration/power registers
        if (oprVal != this->CONFIG_MODE) {
            bool inConfig = false;
            for (U32 attempt = 0; attempt < this->MAX_I2C_RETRIES; attempt++) {
                if (this->writeConfigReg(this->OPR_MODE, this->CONFIG_MODE)) {
                    // Datasheet requires >= 19 ms when switching from any operation mode to CONFIGMODE
                    (void)Os::Task::delay(Fw::TimeInterval(0, 25000));
                    if (this->readConfigReg(this->OPR_MODE, oprVal, 0xF0) &&
                        ((oprVal & 0x0F) == this->CONFIG_MODE)) {
                        inConfig = true;
                        break;
                    }
                }
                (void)Os::Task::delay(Fw::TimeInterval(0, 10000));
            }
            if (!inConfig) {
                msg = "Failed to switch IMU to CONFIGMODE.";
                this->log_WARNING_HI_configError(msg);
                this->configFails++;
                return false;
            }
        }

        // Only write PWR_MODE when needed (while safely in CONFIGMODE)
        if (pwrVal != this->PWR_NORMAL) {
            msg = "Power mode incorrect, updating to NORMAL.";
            this->log_WARNING_LO_configEvent(msg);
            if (!this->writeConfigReg(this->PWR_MODE, this->PWR_NORMAL)) {
                msg = "Failed to write PWR_MODE register.";
                this->log_WARNING_HI_configError(msg);
                this->configFails++;
                return false;
            }
            (void)Os::Task::delay(Fw::TimeInterval(0, 15000));
        }

        // Switch operation mode to NDOF (0x0C) and verify it latched
        msg = "Operation mode incorrect, updating to NDOF.";
        this->log_WARNING_LO_configEvent(msg);

        bool modeSet = false;
        for (U32 attempt = 0; attempt < this->MAX_I2C_RETRIES; attempt++) {
            if (this->writeConfigReg(this->OPR_MODE, this->NDOF)) {
                // Datasheet requires >= 7 ms from CONFIGMODE to NDOF; allow 30 ms for mode transition
                (void)Os::Task::delay(Fw::TimeInterval(0, 30000));
                if (this->readConfigReg(this->OPR_MODE, oprVal, 0xF0) &&
                    ((oprVal & 0x0F) == this->NDOF)) {
                    modeSet = true;
                    break;
                }
            }
            (void)Os::Task::delay(Fw::TimeInterval(0, 10000));
        }

        if (!modeSet) {
            msg = "Failed to set IMU operation mode to NDOF.";
            this->log_WARNING_HI_configError(msg);
            this->configFails++;
            return false;
        }
    }

    // Poll SYS_STATUS until sensor fusion algorithm is running (0x05)
    U8 statusVal = 0xFF;
    for (U32 poll = 0; poll < this->MAX_FUSION_POLLS; poll++) {
        if (this->readConfigReg(this->SYS_STATUS, statusVal, 0xF8)) {
            if (statusVal == this->FUSION_RUNNING) {
                break;
            }
            if (statusVal == this->CONFIG_ERROR) {
                U8 errVal = 0;
                if (this->readConfigReg(this->SYS_ERR, errVal, 0xF0) && errVal != this->NO_ERR) {
                    this->log_WARNING_HI_deviceError(errVal);
                    this->configFails++;
                    return false;
                }
            }
        }
        (void)Os::Task::delay(Fw::TimeInterval(0, 50000));
    }

    if (statusVal != this->FUSION_RUNNING) {
        msg = "System status not *Fusion*. Wait for IMU to finish initialization.";
        this->log_WARNING_LO_configEvent(msg);
        this->configFails++;
        return false;
    }

    this->configFails = 0;

    msg = "IMU is correctly configured.";
    this->log_WARNING_LO_configEvent(msg);
    this->initialized = true;
    return true;
}

void IMUWorkshop ::schedIn_handler(FwIndexType portNum, U32 context) {
    // On every tick, read the sensor data and publish enabled telemetry channels
    if (this->initialized) {
        Fw::Buffer writeBuff = this->allocate_out(0, sizeof(U8));
        if (writeBuff.getSize() < sizeof(U8)) {
            if (writeBuff.isValid()) {
                this->deallocate_out(0, writeBuff);
            }
            this->log_WARNING_LO_MemoryAllocationFailed();
            return;
        }
        writeBuff.setSize(1);

        Fw::Buffer readBuff = this->allocate_out(0, this->NUM_DT_BYT);
        if (readBuff.getSize() < this->NUM_DT_BYT) {
            if (readBuff.isValid()) {
                this->deallocate_out(0, readBuff);
            }
            if (writeBuff.isValid()) {
                this->deallocate_out(0, writeBuff);
            }
            this->log_WARNING_LO_MemoryAllocationFailed();
            return;
        }
        readBuff.setSize(this->NUM_DT_BYT);

        // Read all 45 bytes of sensor data (0x08..0x34) from the IMU
        Drv::I2cStatus status = Drv::I2cStatus::I2C_OTHER_ERR;
        for (U32 attempt = 0; attempt < this->MAX_I2C_RETRIES; attempt++) {
            Fw::ExternalSerializeBufferWithMemberCopy wrSerial = writeBuff.getSerializer();
            (void)wrSerial.serializeFrom(this->DATA_ADDR);
            status = this->i2cWriteRead_out(0, this->ADDRESS, writeBuff, readBuff);
            if (status == Drv::I2cStatus::I2C_OK) {
                break;
            }
            (void)Os::Task::delay(Fw::TimeInterval(0, 5000));
        }
        this->checkStatus(status);
        if (status != Drv::I2cStatus::I2C_OK) {
            if (readBuff.isValid()) {
                this->deallocate_out(0, readBuff);
            }
            if (writeBuff.isValid()) {
                this->deallocate_out(0, writeBuff);
            }
            return;
        }

        this->deserializeData(readBuff);

        // Check if each telemetry channel group is enabled; if so, publish to telemetry
        if (this->tmpEnabled) {
            this->tlmWrite_temp(this->tlmData.temp);
        }

        if (this->gyrEnabled) {
            this->tlmWrite_gyr_x(this->tlmData.gyrX);
            this->tlmWrite_gyr_y(this->tlmData.gyrY);
            this->tlmWrite_gyr_z(this->tlmData.gyrZ);
        }

        if (this->accEnabled) {
            this->tlmWrite_acc_x(this->tlmData.accX);
            this->tlmWrite_acc_y(this->tlmData.accY);
            this->tlmWrite_acc_z(this->tlmData.accZ);
        }

        if (this->magEnabled) {
            this->tlmWrite_mag_x(this->tlmData.magX);
            this->tlmWrite_mag_y(this->tlmData.magY);
            this->tlmWrite_mag_z(this->tlmData.magZ);
        }

        if (this->grvEnabled) {
            this->tlmWrite_grv_x(this->tlmData.grvX);
            this->tlmWrite_grv_y(this->tlmData.grvY);
            this->tlmWrite_grv_z(this->tlmData.grvZ);
        }

        if (this->eulEnabled) {
            this->tlmWrite_eul_x(this->tlmData.eulX);
            this->tlmWrite_eul_y(this->tlmData.eulY);
            this->tlmWrite_eul_z(this->tlmData.eulZ);
        }

        if (this->quaEnabled) {
            this->tlmWrite_qua_x(this->tlmData.quaX);
            this->tlmWrite_qua_y(this->tlmData.quaY);
            this->tlmWrite_qua_z(this->tlmData.quaZ);
        }

        if (this->linAccEnabled) {
            this->tlmWrite_lia_x(this->tlmData.liaX);
            this->tlmWrite_lia_y(this->tlmData.liaY);
            this->tlmWrite_lia_z(this->tlmData.liaZ);
        }

        if (readBuff.isValid()) {
            this->deallocate_out(0, readBuff);
        }
        if (writeBuff.isValid()) {
            this->deallocate_out(0, writeBuff);
        }
    } else {
        this->log_WARNING_HI_configIncomplete();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void IMUWorkshop ::configure_sensor_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->config();
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void IMUWorkshop ::restart_sensor_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->initialized = false;
    this->resets++;
    (void)this->writeConfigReg(this->SYS_TRIGGER, this->RESET_IMU, 1);
    // Allow BNO055 ~650 ms power-on reset time to complete before accepting new I2C traffic
    (void)Os::Task::delay(Fw::TimeInterval(0, 700000));
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_accelerometer_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->accEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_eulerAngles_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->eulEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_magnetometer_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->magEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_gyroscope_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->gyrEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_quaternions_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->quaEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_linearAcceleration_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->linAccEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_gravityVectors_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->grvEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_temperature_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    this->tmpEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop::checkStatus(Drv::I2cStatus i2cStatus) {
    this->calls++;
    switch (i2cStatus) {
        case Drv::I2cStatus::I2C_OK:
            this->log_ACTIVITY_HI_i2cSuccess();
            break;

        case Drv::I2cStatus::I2C_ADDRESS_ERR:
            this->log_WARNING_HI_i2cAddressFailure();
            break;

        case Drv::I2cStatus::I2C_WRITE_ERR:
            this->log_WARNING_HI_i2cWriteError();
            break;

        case Drv::I2cStatus::I2C_READ_ERR:
            this->log_WARNING_HI_i2cReadError();
            break;

        case Drv::I2cStatus::I2C_OPEN_ERR:
            this->log_WARNING_HI_i2cOpenError();
            break;

        case Drv::I2cStatus::I2C_OTHER_ERR:
            this->log_WARNING_HI_i2cOtherError();
            break;

        default:
            this->log_WARNING_HI_i2cOtherError();
            break;
    }
}

bool IMUWorkshop::readConfigReg(U8 addr, U8& val, U8 reservedMask) {
    Fw::Buffer writeBuff = this->allocate_out(0, sizeof(U8));
    if (writeBuff.getSize() < sizeof(U8)) {
        if (writeBuff.isValid()) {
            this->deallocate_out(0, writeBuff);
        }
        this->log_WARNING_LO_MemoryAllocationFailed();
        return false;
    }
    writeBuff.setSize(1);

    Fw::Buffer readBuff = this->allocate_out(0, sizeof(U8));
    if (readBuff.getSize() < sizeof(U8)) {
        if (readBuff.isValid()) {
            this->deallocate_out(0, readBuff);
        }
        if (writeBuff.isValid()) {
            this->deallocate_out(0, writeBuff);
        }
        this->log_WARNING_LO_MemoryAllocationFailed();
        return false;
    }
    readBuff.setSize(1);

    Drv::I2cStatus status = Drv::I2cStatus::I2C_OTHER_ERR;
    bool validRead = false;
    U8 readByte = 0xFF;

    for (U32 attempt = 0; attempt < this->MAX_I2C_RETRIES; attempt++) {
        Fw::ExternalSerializeBufferWithMemberCopy wrSerial = writeBuff.getSerializer();
        (void)wrSerial.serializeFrom(addr);

        status = this->i2cWriteRead_out(0, this->ADDRESS, writeBuff, readBuff);
        if (status == Drv::I2cStatus::I2C_OK) {
            Fw::ExternalSerializeBufferWithMemberCopy rdSerial = readBuff.getDeserializer();
            rdSerial.resetDeser();
            (void)rdSerial.deserializeTo(readByte);

            // Reject bytes where reserved zero-bits were corrupted high (e.g. RPi I2C clock stretching)
            if ((readByte & reservedMask) == 0) {
                validRead = true;
                break;
            }
        }
        (void)Os::Task::delay(Fw::TimeInterval(0, 5000));
    }

    if (readBuff.isValid()) {
        this->deallocate_out(0, readBuff);
    }
    if (writeBuff.isValid()) {
        this->deallocate_out(0, writeBuff);
    }

    if (!validRead) {
        if (status != Drv::I2cStatus::I2C_OK) {
            this->checkStatus(status);
        } else {
            this->log_WARNING_HI_i2cReadError();
        }
        return false;
    }

    this->checkStatus(status);
    val = readByte;
    return true;
}

bool IMUWorkshop::writeConfigReg(U8 addr, U8 byte, U32 maxRetries) {
    Fw::Buffer writeBuff = this->allocate_out(0, sizeof(U8[2]));
    if (writeBuff.getSize() < sizeof(U8[2])) {
        if (writeBuff.isValid()) {
            this->deallocate_out(0, writeBuff);
        }
        this->log_WARNING_LO_MemoryAllocationFailed();
        return false;
    }
    writeBuff.setSize(2);

    Drv::I2cStatus status = Drv::I2cStatus::I2C_OTHER_ERR;
    const U32 tries = (maxRetries > 0) ? maxRetries : 1;

    for (U32 attempt = 0; attempt < tries; attempt++) {
        Fw::ExternalSerializeBufferWithMemberCopy wrSerial = writeBuff.getSerializer();
        (void)wrSerial.serializeFrom(addr);
        (void)wrSerial.serializeFrom(byte);

        status = this->i2cWrite_out(0, this->ADDRESS, writeBuff);
        if (status == Drv::I2cStatus::I2C_OK) {
            break;
        }
        if (attempt + 1 < tries) {
            (void)Os::Task::delay(Fw::TimeInterval(0, 5000));
        }
    }

    this->checkStatus(status);
    if (writeBuff.isValid()) {
        this->deallocate_out(0, writeBuff);
    }

    return (status == Drv::I2cStatus::I2C_OK);
}

F32 IMUWorkshop::readAxisValue(Fw::ExternalSerializeBufferWithMemberCopy& rdSerial, F32 conversionRate) {
    U8 lsb = 0;
    U8 msb = 0;

    // IMU sends data LSB first (little endian)
    (void)rdSerial.deserializeTo(lsb);
    (void)rdSerial.deserializeTo(msb);

    // Shift msb left 8 bits, then OR it with lsb. This gives the data as big-endian
    I16 val = static_cast<I16>(static_cast<U16>(lsb) | (static_cast<U16>(msb) << 8));

    return (static_cast<F32>(val)) / conversionRate;
}

void IMUWorkshop::deserializeData(Fw::Buffer& readBuff) {
    Fw::ExternalSerializeBufferWithMemberCopy rdSerial = readBuff.getDeserializer();
    rdSerial.resetDeser();

    // Deserialize each sensor reading in the exact order the BNO055 returns them (0x08..0x34)
    this->tlmData.accX = this->readAxisValue(rdSerial, this->conversionRates[0]);
    this->tlmData.accY = this->readAxisValue(rdSerial, this->conversionRates[0]);
    this->tlmData.accZ = this->readAxisValue(rdSerial, this->conversionRates[0]);

    this->tlmData.magX = this->readAxisValue(rdSerial, this->conversionRates[1]);
    this->tlmData.magY = this->readAxisValue(rdSerial, this->conversionRates[1]);
    this->tlmData.magZ = this->readAxisValue(rdSerial, this->conversionRates[1]);

    this->tlmData.gyrX = this->readAxisValue(rdSerial, this->conversionRates[2]);
    this->tlmData.gyrY = this->readAxisValue(rdSerial, this->conversionRates[2]);
    this->tlmData.gyrZ = this->readAxisValue(rdSerial, this->conversionRates[2]);

    this->tlmData.eulX = this->readAxisValue(rdSerial, this->conversionRates[3]);
    this->tlmData.eulY = this->readAxisValue(rdSerial, this->conversionRates[3]);
    this->tlmData.eulZ = this->readAxisValue(rdSerial, this->conversionRates[3]);

    // BNO055 quaternion block (0x20..0x27) starts with W before X, Y, Z
    this->tlmData.quaW = this->readAxisValue(rdSerial, this->conversionRates[4]);
    this->tlmData.quaX = this->readAxisValue(rdSerial, this->conversionRates[4]);
    this->tlmData.quaY = this->readAxisValue(rdSerial, this->conversionRates[4]);
    this->tlmData.quaZ = this->readAxisValue(rdSerial, this->conversionRates[4]);

    this->tlmData.liaX = this->readAxisValue(rdSerial, this->conversionRates[5]);
    this->tlmData.liaY = this->readAxisValue(rdSerial, this->conversionRates[5]);
    this->tlmData.liaZ = this->readAxisValue(rdSerial, this->conversionRates[5]);

    this->tlmData.grvX = this->readAxisValue(rdSerial, this->conversionRates[6]);
    this->tlmData.grvY = this->readAxisValue(rdSerial, this->conversionRates[6]);
    this->tlmData.grvZ = this->readAxisValue(rdSerial, this->conversionRates[6]);

    (void)rdSerial.deserializeTo(this->tlmData.temp);
}

}  // namespace Components
