# Running the IMU Workshop

For this workshop, you will be testing an Inertial Measurement Unit (IMU) sensor using the `IMUWorkshopDeployment` F' deployment. This guide covers building the deployment, running it (locally or on a Raspberry Pi), connecting the ground data system (GDS), and using the `IMUWorkshop` component's commands and telemetry.

Before starting, make sure you've completed the environment setup in [setup-workshops.md](./setup-workshops.md).

---

## 1. Build the deployment

The deployment lives at `Workshops/Deployments/IMUWorkshopDeployment`. **You must build and run `fprime-util` from inside that directory** — running it from a parent directory (e.g. `Workshops/Deployments/`) will fail with an error like:

```
ninja: error: unknown target 'Workshops_Deployments'
```

To build:

```
cd Workshops/Deployments/IMUWorkshopDeployment
fprime-util build
```

This produces the deployment binary and a JSON command/telemetry dictionary under:

```
build-artifacts/Linux/Workshops_Deployments_IMUWorkshopDeployment/
├── bin/IMUWorkshopDeployment
└── dict/IMUWorkshopDeploymentTopologyDictionary.json
```

(If you cross-compiled for the Raspberry Pi, substitute the appropriate platform directory in place of `Linux/`.)

---

## 2. Run the deployment

The deployment's communication driver (`comDriver`) is a `Drv.TcpServer` — **the deployment binary itself acts as a TCP server**, and the GDS on your workstation connects to it as a client. The binary takes two command-line arguments:

```
Usage: ./IMUWorkshopDeployment [options]
-a  hostname/IP address
-p  port_number
```

### Running locally (same machine as GDS)

```
cd build-artifacts/Linux/Workshops_Deployments_IMUWorkshopDeployment/bin
./IMUWorkshopDeployment -a 0.0.0.0 -p 50000
```

Then, in another terminal, launch the GDS from the deployment directory (it will auto-start the binary for you if you omit `-n`, or you can point it at the dictionary directly):

```
cd Workshops/Deployments/IMUWorkshopDeployment
fprime-gds
```

### Running on a Raspberry Pi

1. Copy (or build directly on) the Pi and start the deployment binary there, bound to all interfaces:

   ```
   ./IMUWorkshopDeployment -a 0.0.0.0 -p 50000
   ```

2. From your workstation, connect the GDS as a **client** to the Pi's IP address and port, pointing it at the dictionary produced by your build. **Use the full absolute path to your dictionary file** — it must actually exist on disk, e.g.:

   ```
   fprime-gds -n --ip-client --ip-address "pi0.gas.usu.edu" --ip-port 50000 \
       --dictionary /home/<you>/Workshops/FSW/src/Workshops/build-artifacts/Linux/Workshops_Deployments_IMUWorkshopDeployment/dict/IMUWorkshopDeploymentTopologyDictionary.json
   ```

   - `-n` (`--no-app`) tells GDS not to try to launch the deployment itself — it's already running on the Pi.
   - `--ip-client` tells GDS to connect *out* to the Pi rather than open its own server (this is actually the default for the `ip` communication plugin, but it's included here for clarity).
   - `--ip-address` takes the Pi's hostname or IP; `--ip-port` must match the `-p` the deployment binary was started with.

3. Open the GDS web GUI at [http://localhost:5000](http://localhost:5000) (default) to send commands and view telemetry/events.

---

## 3. Using the `IMUWorkshop` component

Once connected, all commands and telemetry below are under the `IMUWorkshop` component prefix in the GDS.

### Startup sequence

The IMU must be configured before it will report data:

1. Send **`IMUWorkshop.configure_sensor`** — runs the configuration sequence (sets power mode, forces `NDOF` fusion mode, checks system status).
2. Until configuration succeeds, `schedIn` ticks will log a `configIncomplete` warning event instead of reading data.
3. Watch for the `configEvent` event confirming *"IMU is correctly configured."*

If something goes wrong, **`IMUWorkshop.restart_sensor`** issues a reset trigger to the IMU (you'll need to `configure_sensor` again afterward).

### Enabling telemetry groups

The IMU is read every tick regardless, but each sensor group is only **published** to telemetry if explicitly enabled. Each of the following commands takes one `bool mode` argument (`true` to enable, `false` to disable):

| Command | Enables |
|---|---|
| `enable_accelerometer(mode)` | `acc_x`, `acc_y`, `acc_z` |
| `enable_magnetometer(mode)` | `mag_x`, `mag_y`, `mag_z` |
| `enable_gyroscope(mode)` | `gyr_x`, `gyr_y`, `gyr_z` |
| `enable_eulerAngles(mode)` | `eul_x`, `eul_y`, `eul_z` |
| `enable_quaternions(mode)` | `qua_x`, `qua_y`, `qua_z` |
| `enable_linearAcceleration(mode)` | `lia_x`, `lia_y`, `lia_z` |
| `enable_gravityVectors(mode)` | `grv_x`, `grv_y`, `grv_z` |
| `enable_temperature(mode)` | `temp` |

All groups default to disabled. For example, to see accelerometer and temperature data, send:

```
IMUWorkshop.enable_accelerometer(mode=true)
IMUWorkshop.enable_temperature(mode=true)
```

### Telemetry channels

| Channel | Type | Description |
|---|---|---|
| `acc_x`, `acc_y`, `acc_z` | F32 | Accelerometer (m/s²) |
| `mag_x`, `mag_y`, `mag_z` | F32 | Magnetometer |
| `gyr_x`, `gyr_y`, `gyr_z` | F32 | Gyroscope |
| `eul_x`, `eul_y`, `eul_z` | F32 | Euler angles |
| `qua_x`, `qua_y`, `qua_z` | F32 | Quaternion components |
| `lia_x`, `lia_y`, `lia_z` | F32 | Linear acceleration (gravity removed) |
| `grv_x`, `grv_y`, `grv_z` | F32 | Gravity vector |
| `temp` | I8 | Temperature |

### Events to watch for

- `configEvent` / `configError` / `configIncomplete` — configuration status
- `deviceError(error)` — IMU reporting an internal error code
- `i2cSuccess` / `i2cAddressFailure` / `i2cWriteError` / `i2cReadError` / `i2cOpenError` / `i2cOtherError` — I2C transaction status on every read/write
- `MemoryAllocationFailed` — buffer pool exhausted (should not occur under normal operation)

---

## Troubleshooting

- **`ninja: error: unknown target 'Workshops_Deployments'`** — you ran `fprime-util build` from the wrong directory. `cd` into `Workshops/Deployments/IMUWorkshopDeployment` first.
- **GDS shows no telemetry** — make sure you've run `configure_sensor` and enabled at least one sensor group with its `enable_*` command.
- **Can't connect to the Pi** — confirm the deployment binary is running and listening (check its console output), confirm the Pi's IP/port, and make sure nothing (e.g. a firewall) is blocking the TCP port between your workstation and the Pi.
