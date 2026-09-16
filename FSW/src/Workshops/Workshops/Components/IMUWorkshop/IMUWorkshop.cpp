// ======================================================================
// \title  IMUWorkshop.cpp
// \author xtilloo
// \brief  cpp file for IMUWorkshop component implementation class
// ======================================================================

#include "Workshops/Components/IMUWorkshop/IMUWorkshop.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

IMUWorkshop ::IMUWorkshop(const char* const compName) : IMUWorkshopComponentBase(compName) {}

IMUWorkshop ::~IMUWorkshop() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void IMUWorkshop ::config() {    
    U8 val;
    Fw::String msg;
    U8 PWR_STATE = 0x00;

    //Check that the power mode is correct
    val = this->readConfigReg(this->PWR_MODE);
    val = val & 0x03; //Only bottom 2 bits hold data

    this->writeConfigReg(this->PWR_MODE, PWR_STATE);    
    
    //Check that the operation mode is correct
    val = this->readConfigReg(this->OPR_MODE);
    val = val & 0x0F;
    if(val != this->NDOF) {
        msg = "Operation mode incorrect, updating to NDOF.";
        this->log_WARNING_LO_configEvent(msg);
        this->writeConfigReg(this->OPR_MODE, this->NDOF);
        return;
    }

    //Check system status and potentially system error
    val = this->readConfigReg(this->SYS_STATUS);
    if(val == this->CONFIG_ERROR) { //If Showing there's an error
        //Check system error
        val = this->readConfigReg(this->SYS_ERR);
        if(val != this->NO_ERR) {
            this->log_WARNING_HI_deviceError(val);
            this->configFails++;
            return;
        }
    }
    else if(val != this->FUSION_RUNNING) {
        msg = 
            "System status not *Fusion*. Wait for IMU to finish initialization.";
        this->log_WARNING_LO_configEvent(msg);
        this->configFails++;
        return;
    }

    this->configFails = 0;
    
    msg = "IMU is correctly configured.";
    this->log_WARNING_LO_configEvent(msg);
    this->initialized = true;
    return;
}

void IMUWorkshop ::schedIn_handler(FwIndexType portNum, U32 context) {
    // On every tick, we are just going to read the sensor data,
    // then log that data to telemetry channels if they are active

    if (this->initialized) {
        //Initialize buffers
        Fw::Buffer writeBuff = this->allocate_out(0,sizeof(U8));

        if(writeBuff.getSize() < sizeof(U8)) {
            if (writeBuff.isValid()) {
                this->deallocate_out(0,writeBuff);
            }
            this->log_WARNING_LO_MemoryAllocationFailed();
            return;
        }
        writeBuff.setSize(1);

        Fw::Buffer readBuff = this->allocate_out(0,this->NUM_DT_BYT);
        if(readBuff.getSize() < this->NUM_DT_BYT) {
            if (readBuff.isValid()) {
                this->deallocate_out(0,readBuff);
            }
            if (writeBuff.isValid()) {
                this->deallocate_out(0,writeBuff);
            }
            this->log_WARNING_LO_MemoryAllocationFailed();
            return;
        }
        readBuff.setSize(this->NUM_DT_BYT);

        // Read all 43 bytes of data from the IMU
        Fw::ExternalSerializeBufferWithMemberCopy 
                wrSerial = writeBuff.getSerializer();
        wrSerial.serializeFrom(this->DATA_ADDR);
        Drv::I2cStatus status = this->i2cWriteRead_out(
            0,
            this->ADDRESS,
            writeBuff,
            readBuff
        );
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

        // Check if each telemetry channel is enabled, if so write to that telemetry channel
        if (tmpEnabled) {
            this->tlmWrite_temp(tlmData.temp);
        }

        if (gyrEnabled) {
            this->tlmWrite_gyr_x(tlmData.gyrX);
            this->tlmWrite_gyr_y(tlmData.gyrY);
            this->tlmWrite_gyr_z(tlmData.gyrZ);
        }

        if (accEnabled) {
            this->tlmWrite_acc_x(tlmData.accX);
            this->tlmWrite_acc_y(tlmData.accY);
            this->tlmWrite_acc_z(tlmData.accZ);
        }

        if (grvEnabled) {
            this->tlmWrite_grv_x(tlmData.grvX);
            this->tlmWrite_grv_y(tlmData.grvY);
            this->tlmWrite_grv_z(tlmData.grvZ);
        }

        if (eulEnabled) {
            this->tlmWrite_eul_x(tlmData.eulX);
            this->tlmWrite_eul_y(tlmData.eulY);
            this->tlmWrite_eul_z(tlmData.eulZ);
        }

        if (quaEnabled) {
            this->tlmWrite_qua_x(tlmData.quaX);
            this->tlmWrite_qua_y(tlmData.quaY);
            this->tlmWrite_qua_z(tlmData.quaZ);
        }

        if (linAccEnabled) {
            this->tlmWrite_lia_x(tlmData.liaX);
            this->tlmWrite_lia_y(tlmData.liaY);
            this->tlmWrite_lia_z(tlmData.liaZ);
        }
        
        if (readBuff.isValid()) {
            this->deallocate_out(0, readBuff);
        }
        if (writeBuff.isValid()) {
            this->deallocate_out(0, writeBuff);
        }
    }
    else {
        this->log_WARNING_HI_configIncomplete();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void IMUWorkshop ::configure_sensor_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    this->config();
}

void IMUWorkshop ::restart_sensor_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->writeConfigReg(this->SYS_TRIGGER, this->RESET_IMU);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}


void IMUWorkshop ::enable_accelerometer_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this->accEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_eulerAngles_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this->eulEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_magnetometer_cmdHandler(FwOpcodeType opCode, U32 cmdSeq,bool mode) {
    // TODO
    this->magEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_gyroscope_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this-> gyrEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_quaternions_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this->quaEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_linearAcceleration_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this->linAccEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_gravityVectors_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this->grvEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop ::enable_temperature_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool mode) {
    // TODO
    this->tmpEnabled = mode;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void IMUWorkshop::checkStatus(Drv::I2cStatus i2cStatus) {
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

  U8 IMUWorkshop::readConfigReg(U8 addr) {
    U8 val = 0xFF;

    //Declare and initialize buffers
    Fw::Buffer writeBuff = this->allocate_out(0,sizeof(U8));
    if(writeBuff.getSize() < sizeof(U8)) {
      if (writeBuff.isValid()) {
        this->deallocate_out(0,writeBuff);
      }
      this->log_WARNING_LO_MemoryAllocationFailed();
      return 0xFF;
    }
    writeBuff.setSize(1);

    Fw::Buffer readBuff = this->allocate_out(0,sizeof(U8));
    if(readBuff.getSize() < sizeof(U8)) {
      if (readBuff.isValid()) {
        this->deallocate_out(0,readBuff);
      }
      if (writeBuff.isValid()) {
        this->deallocate_out(0,writeBuff);
      }
      this->log_WARNING_LO_MemoryAllocationFailed();
      return 0xFF;
    }
    readBuff.setSize(1);
    
    Fw::ExternalSerializeBufferWithMemberCopy 
            rdSerial = readBuff.getDeserializer();
    Fw::ExternalSerializeBufferWithMemberCopy 
            wrSerial = writeBuff.getSerializer();

    // Fetch the value
    wrSerial.resetSer();
    rdSerial.resetDeser();
    wrSerial.serializeFrom(addr);
    Drv::I2cStatus status = this->i2cWriteRead_out(
      0, 
      this->ADDRESS, 
      writeBuff, 
      readBuff
    );
    this->checkStatus(status);
    if (status != Drv::I2cStatus::I2C_OK) {
      if (readBuff.isValid()) {
        this->deallocate_out(0, readBuff);
      }
      if (writeBuff.isValid()) {
        this->deallocate_out(0, writeBuff);
      }
      return 0xFF;
    }
    rdSerial.resetDeser();
    rdSerial.deserializeTo(val);

    if (readBuff.isValid()) {
      this->deallocate_out(0, readBuff);
    }
    if (writeBuff.isValid()) {
      this->deallocate_out(0, writeBuff);
    }

    return val;
  }

  void IMUWorkshop::writeConfigReg(U8 addr, U8 byte) {
    //Declare and initialize buffers
    Fw::Buffer writeBuff = this->allocate_out(0,sizeof(U8 [2]));
    if(writeBuff.getSize() < sizeof(U8 [2])) {
      if (writeBuff.isValid()) {
        this->deallocate_out(0,writeBuff);
      }
      this->log_WARNING_LO_MemoryAllocationFailed();
      return;
    }
    writeBuff.setSize(2);
    
    Fw::ExternalSerializeBufferWithMemberCopy 
            wrSerial = writeBuff.getSerializer();

    // Fetch the value
    wrSerial.resetSer();
    wrSerial.serializeFrom(addr);
    wrSerial.serializeFrom(byte);
    Drv::I2cStatus status = this->i2cWrite_out(0, this->ADDRESS, writeBuff);
    this->checkStatus(status);
    if (status != Drv::I2cStatus::I2C_OK) {
      if (writeBuff.isValid()) {
        this->deallocate_out(0, writeBuff);
      }
      return;
    }

    if (writeBuff.isValid()) {
      this->deallocate_out(0, writeBuff);
    }

    return;
  }

  F32 IMUWorkshop::readAxisValue(Fw::ExternalSerializeBufferWithMemberCopy& rdSerial, F32 conversionRate) {
    U8 lsb, msb;

    // IMU sends data LSB first (little endian)
    rdSerial.deserializeTo(lsb);
    rdSerial.deserializeTo(msb);

    // Shift msb left 8 bits, then OR it with lsb. This gives the data as big-endian
    I16 val = static_cast<I16>(static_cast<U16>(lsb) | (static_cast<U16>(msb) << 8));

    return (static_cast<F32>(val)) / conversionRate;
  }

  void IMUWorkshop::deserializeData(Fw::Buffer& readBuff) {
    Fw::ExternalSerializeBufferWithMemberCopy
            rdSerial = readBuff.getDeserializer();
    rdSerial.resetDeser();

    // Deserialize each F32 sensor reading directly into its named field,
    // in the same order the IMU sends them
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

    this->tlmData.quaX = this->readAxisValue(rdSerial, this->conversionRates[4]);
    this->tlmData.quaY = this->readAxisValue(rdSerial, this->conversionRates[4]);
    this->tlmData.quaZ = this->readAxisValue(rdSerial, this->conversionRates[4]);

    this->tlmData.liaX = this->readAxisValue(rdSerial, this->conversionRates[5]);
    this->tlmData.liaY = this->readAxisValue(rdSerial, this->conversionRates[5]);
    this->tlmData.liaZ = this->readAxisValue(rdSerial, this->conversionRates[5]);

    this->tlmData.grvX = this->readAxisValue(rdSerial, this->conversionRates[6]);
    this->tlmData.grvY = this->readAxisValue(rdSerial, this->conversionRates[6]);
    this->tlmData.grvZ = this->readAxisValue(rdSerial, this->conversionRates[6]);

    rdSerial.deserializeTo(this->tlmData.temp);
  }

}  // namespace Components
