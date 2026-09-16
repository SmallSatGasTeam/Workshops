// ======================================================================
// \title  IMUWorkshop.hpp
// \author xtilloo
// \brief  hpp file for IMUWorkshop component implementation class
// ======================================================================

#ifndef Components_IMUWorkshop_HPP
#define Components_IMUWorkshop_HPP

#include "Workshops/Components/IMUWorkshop/IMUWorkshopComponentAc.hpp"

namespace Components {

class IMUWorkshop final : public IMUWorkshopComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct IMUWorkshop object
    IMUWorkshop(const char* const compName  //!< The component name
    );

    //! Destroy IMUWorkshop object
    ~IMUWorkshop();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for schedIn
    //!
    //! SchedIn: Called by the rate group to periodically retrieve data
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    //! checkStatus
    //!
    //! Calls an event based on the status returned from the read/write operation
    void checkStatus(Drv::I2cStatus status);

    //! config
    //!
    //! Summary: Runs the configuration code
    void config();

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

    //! deserializeData
    //!
    //! Summary: Deserializes the IMU telemetry from the read buffer and stores it
    //!          into tlmData, without publishing to telemetry channels.
    //! Parameters: Fw::Buffer& readBuff, The buffer containing raw read telemetry data.
    void deserializeData(Fw::Buffer& readBuff);

    //! readAxisValue
    //!
    //! Summary: Deserializes one little-endian I16 sensor reading from rdSerial
    //!          and scales it by conversionRate.
    F32 readAxisValue(Fw::ExternalSerializeBufferWithMemberCopy& rdSerial, F32 conversionRate);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command configure_sensor
    void configure_sensor_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq            //!< The command sequence number
                                     ) override;

    //! Handler implementation for command configure_sensor
    void restart_sensor_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq            //!< The command sequence number
                                     ) override;

    //! Handler implementation for command enable_accelerometer
    void enable_accelerometer_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                         U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                         ) override;

    //! Handler implementation for command enable_magnetometer
    void enable_magnetometer_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                        U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                        ) override;

    //! Handler implementation for command enable_gyroscope
    void enable_gyroscope_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                     ) override;

    //! Handler implementation for command enable_quaternions
    void enable_quaternions_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                       ) override;

    //! Handler implementation for command enable_linearAcceleration
    void enable_linearAcceleration_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                              U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                              ) override;

    //! Handler implementation for command enable_gravityVectors
    void enable_gravityVectors_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                          U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                          ) override;

    //! Handler implementation for command enable_temperature
    void enable_temperature_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                       ) override;

    //! Handler implementation for command enable_eulerAngles
    void enable_eulerAngles_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq,           //!< The command sequence number
                                         bool mode
                                       ) override;

  private:
    // ----------------------------------------------------------------------
    // Telemetry storage
    // ----------------------------------------------------------------------

    //! Deserialized IMU sensor readings, one named field per axis, in the
    //! exact order the IMU returns them starting at DATA_ADDR (3 axes per
    //! reading, followed by a single temperature byte).
    struct ImuTlmData {
      F32 accX, accY, accZ;
      F32 magX, magY, magZ;
      F32 gyrX, gyrY, gyrZ;
      F32 eulX, eulY, eulZ;
      F32 quaX, quaY, quaZ;
      F32 liaX, liaY, liaZ;
      F32 grvX, grvY, grvZ;
      I8 temp;
    };

    //! Most recently deserialized sensor readings
    ImuTlmData tlmData;

  private:

    // ----------------------------------------------------------------------
    // Helper Variables
    // ----------------------------------------------------------------------

    // Bools

    bool tmpEnabled = false;
    bool accEnabled = false;
    bool gyrEnabled = false;
    bool magEnabled = false;
    bool quaEnabled = false;
    bool grvEnabled = false;
    bool eulEnabled = false;
    bool linAccEnabled = false;


    //IMU Address Constants
    static constexpr U8 ADDRESS = 0x28;
    static constexpr U8 DATA_ADDR = 0x08;
    static constexpr U8 PWR_MODE = 0X3E;
    static constexpr U8 OPR_MODE = 0x3D;
    static constexpr U8 SYS_STATUS = 0x39;
    static constexpr U8 SYS_ERR = 0x3A;
    static constexpr U8 SYS_TRIGGER = 0x3F;

    //Other Constants
    static constexpr U32 NUM_DT_BYT = 43;
    static constexpr U8 NDOF = 0x0C;
    static constexpr U8 FUSION_RUNNING = 0x05;
    static constexpr U8 NO_ERR = 0x00;
    static constexpr U8 RESET_IMU = 0x20;
    static constexpr U8 CONFIG_ERROR = 0x01;

    //Variables
    U32 calls;
    U32 configFails;
    U32 resets;
    bool initialized;

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

};

}  // namespace Components

#endif
