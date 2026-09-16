module Components {
    @ Component that gathers IMU data
    passive component IMUManager {

        

        #############################################################
        # Ports
        #############################################################

        @ Called by the rate group to periodically retrieve data. Guarded (not
        @ sync) because config() — reachable from here and from configIn/
        @ configureImu — mutates shared state and issues blocking I2C
        @ transactions; guarded serializes all three entry points.
        guarded input port schedIn: Svc.Sched

        @ configIn: triggered by FlightLogic to (re)run the IMU configuration sequence
        guarded input port configIn: FL.ping

        @ Writes I2CData: Drv.I2c
        output port i2cWrite: Drv.I2c

        output port i2cWriteRead: Drv.I2cWriteRead

        #############################################################
        # Commands
        #############################################################

        @ Ground-commandable (re)configuration of the IMU. Same effect as configIn.
        guarded command configureImu() opcode 0x01

        #############################################################
        # telemetry
        #############################################################

        telemetry acc_x : F32 format "{.2f}"
        telemetry acc_y : F32 format "{.2f}" 
        telemetry acc_z : F32 format "{.2f}"

        telemetry mag_x : F32 format "{.2f}"
        telemetry mag_y : F32 format "{.2f}"
        telemetry mag_z : F32 format "{.2f}"

        telemetry gyr_x : F32 format "{.2f}"
        telemetry gyr_y : F32 format "{.2f}"
        telemetry gyr_z : F32 format "{.2f}"

        telemetry eul_x : F32 format "{.2f}"
        telemetry eul_y : F32 format "{.2f}"
        telemetry eul_z : F32 format "{.2f}"

        telemetry qua_x : F32 format "{.2f}"
        telemetry qua_y : F32 format "{.2f}"
        telemetry qua_z : F32 format "{.2f}"

        telemetry lia_x : F32 format "{.2f}"
        telemetry lia_y : F32 format "{.2f}"
        telemetry lia_z : F32 format "{.2f}"

        telemetry grv_x : F32 format "{.2f}"
        telemetry grv_y : F32 format "{.2f}"
        telemetry grv_z : F32 format "{.2f}"

        telemetry temp : I8

        #############################################################
        # Events
        #############################################################

        include "../fppTemplates/i2cEvents.fppi"

        @ Configuration events
        event configEvent(msg: string) \
            severity warning low \
            format "Config Event: {}"

        event configError(msg: string) \
            severity warning high \
            format "Config Error: {}"

        event deviceError(error: U16) \
            severity warning high \
            format "Device register returning error: {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

        @ Allocation port for a buffer
        output port allocate: Fw.BufferGet

        @ Deallocation port for buffers
        output port deallocate: Fw.BufferSend
    }
}