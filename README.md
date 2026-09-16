# Workshops

General location where teams can store content, code, and demos for workshops.

## What's here

### `FSW/` — Flight Software Workshop

An [F' (F Prime)](https://fprime.jpl.nasa.gov/) flight software project used to teach hands-on FSW development. The current workshop centers on an Inertial Measurement Unit (IMU) sensor, driven over I2C (BNO055-style 9-DOF IMU).

```
FSW/
├── docs/
│   ├── setup-workshops.md    # Install Git, a compiler, Python/venv, and clone the repo
│   └── running-workshops.md  # Build, run (locally or on a Raspberry Pi), and use the IMU deployment
└── src/Workshops/            # The F' project root (settings.ini lives here)
    ├── lib/fprime/           # F' framework (git submodule)
    └── Workshops/
        ├── Components/
        │   └── IMUWorkshop/  # Teaching component for the workshop — some
        │                     # handlers are left as exercises for participants
        └── Deployments/
            └── IMUWorkshopDeployment/  # The buildable/runnable F' deployment
                                         # that wires IMUWorkshop into a topology
```

- **`IMUWorkshop`** is the component participants work on during the workshop — it drives a BNO055-style 9-DOF IMU over I2C (configuration/read sequencing), stores decoded sensor readings in a plain struct (`tlmData`), and gates which telemetry channels get published behind per-sensor `enable_*` ground commands.
- **`IMUWorkshopDeployment`** is the deployment that runs `IMUWorkshop` on real hardware (e.g. a Raspberry Pi with an I2C IMU attached) and exposes it to a ground data system (GDS) over TCP.

### Getting started

1. Follow [`FSW/docs/setup-workshops.md`](FSW/docs/setup-workshops.md) to install prerequisites (Git, a compiler, Python 3.10+/venv) and clone this repository.
2. Follow [`FSW/docs/running-workshops.md`](FSW/docs/running-workshops.md) to build `IMUWorkshopDeployment`, run it (locally or on a Pi), connect the GDS, and exercise the IMU commands/telemetry.

## License

See [LICENSE](LICENSE).
