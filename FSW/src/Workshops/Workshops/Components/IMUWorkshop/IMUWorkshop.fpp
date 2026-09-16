module Components {
    @ This is the component that will be used to demo the IMU sensor during the F' workshop
    active component IMUWorkshop {

        ##############################################################################
        #### Commands
        ##############################################################################

        async command configure_sensor

        async command enable_accelerometer(mode: bool)

        async command enable_magnetometer(mode: bool)

        async command enable_gyroscope(mode: bool)

        async command enable_quaternions(mode: bool)

        async command enable_linearAcceleration(mode: bool)

        async command enable_gravityVectors(mode: bool)

        async command enable_temperature(mode: bool)

        async command enable_eulerAngles(mode: bool)

        async command restart_sensor

        ##############################################################################
        #### Telemetry Values
        ##############################################################################
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

        ##############################################################################
        #### Events
        ##############################################################################

        event configEvent(msg: string) \
            severity warning low \
            format "Config Event: {}"

        event configError(msg: string) \
            severity warning high \
            format "Config Error: {}"

        event configIncomplete \
            severity warning high \
            format "You need to configure the sensor first!"

        event deviceError(error: U16) \
            severity warning high \
            format "Device register returning error: {}"

        @The next 6 events are used to check for read and write errors when interfacing with an i2c device
        event i2cSuccess \
            severity activity high \
            format "There was a successful I2C transmission."

        event i2cAddressFailure \
            severity warning high \
            format "Invalid Address"

        event i2cWriteError \
            severity warning high \
            format "Write Failed"

        event i2cReadError \
            severity warning high \
            format "Read Failed"

        event i2cOpenError \
            severity warning high \
            format "Failed to open device"

        event i2cOtherError \
            severity warning high \
            format "Other"

        @ Allocation failed event
        event MemoryAllocationFailed() severity warning low format "Failed to allocate memory"

        ##############################################################################
        #### Ports 
        ##############################################################################

        @ SchedIn: Called by the rate group to periodically retrieve data
        sync input port schedIn: Svc.Sched

        @ i2cWrite: Used to write things to the i2c bus 
        output port i2cWrite: Drv.I2c

        @ i2cWriteRead: Used to write then read things from the i2c bus
        output port i2cWriteRead: Drv.I2cWriteRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

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