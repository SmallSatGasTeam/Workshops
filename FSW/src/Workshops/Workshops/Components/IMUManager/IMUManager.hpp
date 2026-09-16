// ======================================================================
// \title  IMUManager.hpp
// \author Devin 
// \brief  hpp file for IMUManager component implementation class
// ======================================================================

#ifndef Components_imuInterface_HPP
#define Components_imuInterface_HPP

#include "Components/IMUManager/IMUManagerComponentAc.hpp"
#include "Components/componentConfig/Constants.hpp"
#include <vector>
#include <functional>

namespace Components {

  class IMUManager :
    public IMUManagerComponentBase
  {

    public:

      // ----------------------------------------------------------------------
      // Component construction and destruction
      // ----------------------------------------------------------------------

      //! Construct imuInterface object
      IMUManager(
          const char* const compName //!< The component name
      );

      //! Destroy imuInterface object
      ~IMUManager();

      //! Startup
      //!
      //! Configures the IMU in NDOF mode and checks that the configuration was
      //! successful.
      void startup();

    private:

      // ----------------------------------------------------------------------
      // Handler implementations for user-defined typed input ports
      // ----------------------------------------------------------------------

      //! Handler implementation for schedIn
      //!
      //! schedIn: receives a ping from the data collector to send out data
      void schedIn_handler(
          FwIndexType portNum, //!< The port number
          U32 value
      ) override;

      //! Handler implementation for configIn
      //!
      //! configIn: triggered by FlightLogic to (re)run the IMU configuration sequence
      void configIn_handler(
          FwIndexType portNum //!< The port number
      ) override;

      // ----------------------------------------------------------------------
      // Handler implementations for commands
      // ----------------------------------------------------------------------

      //! Handler implementation for command configureImu
      //!
      //! Ground-commandable (re)configuration of the IMU. Same effect as configIn.
      void configureImu_cmdHandler(
          FwOpcodeType opCode, //!< The opcode
          U32 cmdSeq //!< The command sequence number
      ) override;

    private:

      // ----------------------------------------------------------------------
      // Helper Functions
      // ----------------------------------------------------------------------

      //! checkStatus
      //!
      //! Calls an event based on the status returned from the read/write operation
      void checkStatus(Drv::I2cStatus status);

      //! readConfigReg
      //!
      //! Summary: Reads one byte from a configuration register
      //! Parameters: U8 addr, Address of register to read from
      //! Return: Byte received from register at "addr" (U8)
      U8 readConfigReg(U8 addr);

      //! readConfigReg
      //!
      //! Summary: Writes one byte from a configuration register
      //! Parameters: 
      //!   - U8 addr, Address of register to write to
      //!   - U8 byte, Data to be written
      void writeConfigReg(U8 addr, U8 byte);

      //! config
      //!
      //! Summary: Runs the configuration code
      void config();

      //! deserializeAndPublish
      //!
      //! Deserializes the IMU telemetry from the read buffer and writes to telemetry channels.
      //! \param readBuff The Fw::Buffer containing the raw read telemetry data.
      void deserializeAndPublish(Fw::Buffer& readBuff);

    private:

      // ----------------------------------------------------------------------
      // Helper Variables
      // ----------------------------------------------------------------------
      
      //IMU Address Constants
      const U8 ADDRESS = 0x28;
      const U8 DATA_ADDR = 0x08;
      const U8 PWR_MODE = 0X3E;
      const U8 OPR_MODE = 0x3D;
      const U8 SYS_STATUS = 0x39;
      const U8 SYS_ERR = 0x3A;
      const U8 SYS_TRIGGER = 0x3F;

      //Other Constants
      const U32 NUM_DT_BYT = 43;
      const U8 NDOF = 0x0C;
      const U8 FUSION_RUNNING = 0x05;
      const U8 NO_ERR = 0x00;
      const U8 RESET_IMU = 0x20;
      const U8 CONFIG_ERROR = 0x01;

      //Variables
      U32 calls;
      U32 configFails;
      U32 resets;
      bool initialized;

      //Arrays
      std::vector<std::function<void(F32)>> tlmFuncs;

      /*
        [0] = ACC
        [1] = MAG
        [2] = GYR
        [3] = EUL
        [4] = QUA
        [5] = LIA
        [6] = GRV
      */
      F32 conversionRates [7] = {
        100.0f,
        16.0f,
        16.0f,
        16.0f,
        16384.0f,
        100.0f,
        100.0f
      };



    private: 
      // ----------------------------------------------------------------------
      // Enum Types
      // ----------------------------------------------------------------------
      enum PWR_ENUM {
        NORMAL = 0,
        LOW = 1,
        SUSPEND = 2
      } pwrState;


  };

}

#endif
