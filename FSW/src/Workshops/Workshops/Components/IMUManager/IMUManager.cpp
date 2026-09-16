// ======================================================================
// \title  IMUManager.cpp
// \author Devin
// \brief  cpp file for imuInterface component implementation class
// ======================================================================

#include "Components/IMUManager/IMUManager.hpp"
#include "Components/componentConfig/Constants.hpp"

namespace Components {

  // ----------------------------------------------------------------------
  // Component construction and destruction
  // ----------------------------------------------------------------------

  IMUManager ::
    IMUManager(const char* const compName) :
      IMUManagerComponentBase(compName), calls(0), pwrState(NORMAL), 
      initialized(false), configFails(0), resets(0)
  {
    this->tlmFuncs = {
      [this] (F32 a) {tlmWrite_acc_x(a);},
      [this] (F32 a) {tlmWrite_acc_y(a);},
      [this] (F32 a) {tlmWrite_acc_z(a);},
      [this] (F32 a) {tlmWrite_mag_x(a);},
      [this] (F32 a) {tlmWrite_mag_y(a);},
      [this] (F32 a) {tlmWrite_mag_z(a);},
      [this] (F32 a) {tlmWrite_gyr_x(a);},
      [this] (F32 a) {tlmWrite_gyr_y(a);},
      [this] (F32 a) {tlmWrite_gyr_z(a);},
      [this] (F32 a) {tlmWrite_eul_x(a);},
      [this] (F32 a) {tlmWrite_eul_y(a);},
      [this] (F32 a) {tlmWrite_eul_z(a);},
      [this] (F32 a) {tlmWrite_qua_x(a);},
      [this] (F32 a) {tlmWrite_qua_y(a);},
      [this] (F32 a) {tlmWrite_qua_z(a);},
      [this] (F32 a) {tlmWrite_lia_x(a);},
      [this] (F32 a) {tlmWrite_lia_y(a);},
      [this] (F32 a) {tlmWrite_lia_z(a);},
      [this] (F32 a) {tlmWrite_grv_x(a);},
      [this] (F32 a) {tlmWrite_grv_y(a);},
      [this] (F32 a) {tlmWrite_grv_z(a);}
    };

  }

  IMUManager ::
    ~IMUManager()
  {}

  // ----------------------------------------------------------------------
  // Handler implementations for user-defined typed input ports
  // ----------------------------------------------------------------------

  void IMUManager ::startup() {
    config();
    return;
  }

  void IMUManager ::
    schedIn_handler(
        FwIndexType portNum,
        U32 value
    )
  {
    //Return if configuration failed
    if(this->resets >= MAX_RESETS) return;

    //Run startup if not already initialized
    if(!this->initialized) {
      config();
      return;
    }

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

    this->deserializeAndPublish(readBuff);

    if (readBuff.isValid()) {
      this->deallocate_out(0, readBuff);
    }
    if (writeBuff.isValid()) {
      this->deallocate_out(0, writeBuff);
    }
  }

  void IMUManager ::
    configIn_handler(
        FwIndexType portNum
    )
  {
    config();
  }

  void IMUManager ::
    configureImu_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq
    )
  {
    config();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
  }

  void IMUManager ::
    deserializeAndPublish(Fw::Buffer& readBuff)
  {
    //Deserialize the data into each relevant telemetry channel
    Fw::ExternalSerializeBufferWithMemberCopy 
            rdSerial = readBuff.getDeserializer();
    rdSerial.resetDeser();
    I16 val;
    F32 floating;
    U8 type = 0;
    U8 counter = 0;
    U8 lsb, msb;

    //Iterate through the array of pointers to tlmWrite_... functions
    for(std::function<void(F32)> tlmWrite : this->tlmFuncs) {
      // IMU sends data LSB first (little endian)
      rdSerial.deserializeTo(lsb);
      rdSerial.deserializeTo(msb);

      // Shift msb left 8 bits, then OR it with lsb. This gives the data as big-endian
      val = static_cast<I16>(static_cast<U16>(lsb) | (static_cast<U16>(msb) << 8));

      floating = (static_cast<F32>(val))/conversionRates[type];
      tlmWrite(floating);
      counter++;
      //After reading all 3 axis, change type to get correct conversion rate
      if(counter == 3) {
        counter = 0;
        type++;
      }
    }

    I8 temp;
    rdSerial.deserializeTo(temp);
    this->tlmWrite_temp(temp);
  }

  // ----------------------------------------------------------------------
  // Helper Functions
  // ----------------------------------------------------------------------

  void IMUManager ::config() {
    // Don't even try if we've reset too many times
    
    if(this->resets >= MAX_RESETS) return;

    U8 val;
    Fw::String msg;

    //Check that the power mode is correct
    val = this->readConfigReg(this->PWR_MODE);
    val = val & 0x03; //Only bottom 2 bits hold data
    //Check IMU power mode with satellite pwrState set by Flight Logic
    if(val != static_cast<U8>(this->pwrState)) { 
      const char* pwrStateStr = (this->pwrState == LOW) ? "LOW" : 
                                (this->pwrState == SUSPEND) ? "SUSPEND" : "NORMAL";
      char buf[128];
      (void) snprintf(buf, sizeof(buf), "Power mode incorrect, updating power state to %s", pwrStateStr);
      msg = buf;
      this->log_WARNING_LO_configEvent(msg);
      this->writeConfigReg(this->PWR_MODE, static_cast<U8>(this->pwrState));
      return;
    }
    
    //Check that the operation mode is correct
    val = this->readConfigReg(this->OPR_MODE);
    val = val & 0x0F;
    if(val != this->NDOF) {
      msg = "Operation mode incorrect, updating to NDOF.";
      this->log_WARNING_LO_configEvent(msg);
      this->writeConfigReg(this->OPR_MODE, this->NDOF);
      return;
    }

    //Check for too many config fails
    if(this->configFails >= MAX_CONFIG_FAILS) {
      msg = "System status still not *Fusion*, resetting IMU.";
      this->log_WARNING_LO_configEvent(msg);
      this->writeConfigReg(this->SYS_TRIGGER, this->RESET_IMU);
      this->resets++;
      if(this->resets >= MAX_RESETS) {
        msg = "Failed to configure the IMU.";
        this->log_WARNING_HI_configError(msg);
      }
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
  
  void IMUManager::checkStatus(Drv::I2cStatus i2cStatus) {
    if(this->calls < MAX_BACKGROUND_MESSAGES) {
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
          break;
      }
    }
  }

  U8 IMUManager::readConfigReg(U8 addr) {
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

  void IMUManager::writeConfigReg(U8 addr, U8 byte) {
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
}
