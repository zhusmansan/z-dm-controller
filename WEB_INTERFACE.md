# Damaio Motor Controller Web Interface

## Overview
This project adds a modern web-based control interface for the Damaio motor controller running on ESP32. The interface communicates with the controller via **WebSocket** for real-time, low-latency bidirectional communication.

## Features

### Real-time Motor Control
- **Enable/Disable Motor**: One-click motor activation and deactivation
- **Torque Command**: Set motor torque (-28.0 to +28.0 Nm) via slider or numeric input
- **Proportional Gain (Kp)**: Adjust PID controller Kp value (0 to 500)
- **Derivative Gain (Kd)**: Adjust PID controller Kd value (0 to 5)
- **Special Commands**: Clear errors and set zero position

### Live Status Monitoring
- Motor state (Disabled, Enabled, Error states)
- Real-time position (radians)
- Real-time velocity (rad/s)
- Real-time torque (Nm)
- MOS temperature (°C)
- Rotor temperature (°C)

### User Interface
- Modern, responsive web dashboard
- Mobile-friendly design
- Real-time connection status indicator
- Intuitive slider controls and numeric inputs
- Color-coded status and control panels

## Technical Architecture

### WebSocket Communication
The web interface communicates with the ESP32 controller via WebSocket at `/ws` endpoint.

#### Message Format (Browser → ESP32)
```json
{
  "type": "control",
  "action": "enable|disable|torque|kp|kd",
  "value": number
}
```

```json
{
  "type": "command",
  "action": "clear_error|set_zero"
}
```

#### Message Format (ESP32 → Browser)
```json
{
  "type": "status",
  "state": number,
  "position": float,
  "velocity": float,
  "torque": float,
  "t_mos": float,
  "t_rotor": float
}
```

### File Structure
```
main/
├── http_server.c           # HTTP/WebSocket server implementation
├── http_server.h           # Server header file
├── main.c                  # Main application with WiFi and server init
├── web/
│   ├── index.html          # Dashboard UI
│   ├── styles.css          # Responsive styling
│   └── app.js              # WebSocket client and UI logic
├── dm_motor.c              # Motor control implementation
├── dm_motor.h              # Motor control definitions
└── CMakeLists.txt          # Build configuration
```

## Building and Running

### Prerequisites
- ESP-IDF v4.1 or later
- GCC toolchain for ESP32
- Required components: esp_http_server, cjson, esp_wifi, esp_netif

### Build Steps
```bash
cd z-dm-controller.worktrees/agents-motor-control-webpage-addition
idf.py build
idf.py flash
idf.py monitor
```

### WiFi Configuration
The device starts a WiFi Access Point:
- **SSID**: `DamiaoMotor`
- **Password**: `12345678`
- **IP Address**: `192.168.4.1`

To access the web interface:
1. Connect to WiFi network `DamiaoMotor`
2. Open browser and navigate to `http://192.168.4.1`

## Dependencies
- **esp_http_server**: HTTP server with WebSocket support
- **cjson**: JSON parsing library
- **esp_wifi**: WiFi functionality
- **esp_netif**: Network interface abstraction
- **freertos**: Real-time OS kernel

## Motor Control Limits
- **Position**: -12.5 to +12.5 radians
- **Velocity**: -10.0 to +10.0 rad/s
- **Torque**: -28.0 to +28.0 Nm
- **Kp**: 0 to 500
- **Kd**: 0 to 5

## Connection Status
- 🟢 **Green dot**: Connected to motor controller
- 🔴 **Red pulsing dot**: Disconnected (auto-reconnect enabled)

## Troubleshooting

### Cannot Connect to WebSocket
1. Ensure device is on WiFi network `DamiaoMotor`
2. Check device is powered and running
3. Monitor serial output for errors: `idf.py monitor`

### Motor Not Responding
1. Check motor connection via CAN bus
2. Use "Clear Error" button to reset error state
3. Verify motor is enabled (use "Enable Motor" button)

### Status Not Updating
1. Check WebSocket connection status
2. Ensure motor control loop is running (motor should respond to torque commands)
3. Check browser console for JavaScript errors (F12 in most browsers)

## Performance
- **Status Update Rate**: 50ms (20 Hz)
- **Control Command Latency**: < 20ms
- **WebSocket Frame Size**: Max 512 bytes

## Future Enhancements
- Multi-motor support
- Motor diagnostics and logs
- Trajectory recording and playback
- Advanced PID tuning interface
- Motor parameter calibration
- Thermal management visualization

## License
SPDX-License-Identifier: Apache-2.0
