# Quick Start Guide - Motor Control Web Interface

## Setup (5 minutes)

### 1. Build the Project
```bash
cd z-dm-controller.worktrees/agents-motor-control-webpage-addition
idf.py build
```

### 2. Flash to ESP32
```bash
idf.py flash
idf.py monitor
```

### 3. Access the Dashboard
1. Look for WiFi network: **DamiaoMotor**
2. Password: **12345678**
3. Connect your device (laptop, phone, tablet)
4. Open browser: **http://192.168.4.1**

## Using the Web Interface

### Control Panel
- **Enable Motor**: Click to activate the motor
- **Disable Motor**: Click to stop the motor
- **Torque Command**: Use slider or type value (-28 to +28 Nm)
- **Proportional Gain (Kp)**: Adjust controller responsiveness (0-500)
- **Derivative Gain (Kd)**: Adjust damping (0-5)
- **Clear Error**: Reset motor error state
- **Set Zero Position**: Calibrate zero position

### Status Display
Real-time updates show:
- **State**: Current motor state (Enabled, Disabled, etc.)
- **Position**: Motor angle in radians
- **Velocity**: Rotation speed in rad/s
- **Torque**: Applied torque in Nm
- **MOS Temp**: Power stage temperature
- **Rotor Temp**: Motor winding temperature

### Connection Indicator
- 🟢 **Green**: Connected and communicating
- 🔴 **Red/Pulsing**: Disconnected (auto-reconnecting)

## Common Tasks

### Start Motor with Constant Torque
1. Set Torque value (e.g., 2.0)
2. Click **Enable Motor**
3. Motor will maintain that torque

### Adjust Motor Response
1. Use **Proportional Gain (Kp)** slider for faster response
2. Use **Derivative Gain (Kd)** slider for less overshoot
3. Observe motor behavior in real-time

### Reset on Error
1. Click **Clear Error** button
2. Motor will attempt recovery
3. Watch Status display for state change

### Calibrate Position Zero
1. Move motor to desired zero position manually (if needed)
2. Click **Set Zero Position**
3. Motor will record this as zero

## Troubleshooting

### Cannot Connect to WiFi
- Ensure WiFi is enabled on your device
- WiFi network appears as: **DamiaoMotor**
- Check password: **12345678**

### Cannot Access Dashboard
- Try: **http://192.168.4.1**
- Check browser shows secure/insecure warning (normal)
- Try refreshing (Ctrl+F5 or Cmd+Shift+R)

### Status Not Updating
- Check browser console (F12) for errors
- Check connection indicator
- Try refreshing page if disconnected

### Motor Not Responding
- Click **Enable Motor** first
- Check motor is actually enabled (state shows "Enabled")
- Verify CAN bus connection
- Use **Clear Error** if red state shown

## Technical Details

### Communication
- Protocol: WebSocket (real-time bidirectional)
- Update Rate: 20 times per second
- Latency: <20ms typically

### Browser Compatibility
- Chrome/Chromium: ✅ Full support
- Firefox: ✅ Full support
- Safari: ✅ Full support
- Edge: ✅ Full support
- Mobile browsers: ✅ Responsive design

### Limits
- Max torque: ±28 Nm
- Max velocity: ±10 rad/s
- Max position: ±12.5 rad
- Max Kp: 500
- Max Kd: 5

## Network

### WiFi Access Point Details
- **SSID**: DamiaoMotor
- **Password**: 12345678
- **IP Address**: 192.168.4.1
- **Port**: 80 (HTTP/WebSocket)
- **Max Connections**: 4

## Performance

### Update Intervals
- Motor status: Every 50ms (20 Hz)
- Control latency: <20ms
- Reconnection retry: 3 seconds
- Max reconnection attempts: 5

### Data Throughput
- Status message: ~150 bytes
- Command message: ~50 bytes
- Max WebSocket frame: 512 bytes

## Advanced Usage

### Custom Trajectories
While the interface doesn't record trajectories directly, you can:
1. Manually adjust Torque slider to follow desired path
2. Monitor Position display for feedback
3. Adjust Kp/Kd for better tracking

### Performance Tuning
1. Start with low Kp (e.g., 10)
2. Gradually increase until motor responds quickly
3. If oscillating, increase Kd slightly
4. Find optimal balance for your application

### Temperature Monitoring
- Green: Normal (<50°C)
- Yellow: Warm (50-70°C)
- Red: Hot (>70°C) - consider motor rest

## Mobile Use

The interface is fully responsive:
- Works on phones and tablets
- Touch-friendly controls
- Landscape and portrait modes
- Full functionality on mobile

## Support

For issues or questions:
1. Check WEB_INTERFACE.md in project root
2. Review motor state messages for error codes
3. Check ESP32 serial monitor output
4. Verify WiFi connection is stable

---
**Last Updated**: 2025-06-04
**Compatible With**: ESP-IDF 4.1+, Damaio Motor Controller
