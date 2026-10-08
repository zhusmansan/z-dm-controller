class MotorController {
    constructor() {
        this.ws = null;
        this.wsUrl =`ws://${window.location.protocol.startsWith("http")
    ? window.location.hostname : "192.168.1.7"}/ws`;
//  `ws://${window.location.host}/ws`;
        this.reconnectAttempts = 0;
        this.reconnectTimer = null;
        this.initialReconnectDelay = 1000;
        this.maxReconnectDelay = 30000;
        this.wsMessageTimer = null;
        this.wsMessageTimeout = 1500;
        
        this.init();
    }

    init() {
        this.connectWebSocket();
        this.setupEventListeners();
        window.addEventListener('online', () => {
            if (this.reconnectTimer !== null) {
                clearTimeout(this.reconnectTimer);
                this.reconnectTimer = null;
            }
            this.connectWebSocket();
        });
    }

    connectWebSocket() {
        if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN)) {
            return;
        }

        try {
            const socket = new WebSocket(this.wsUrl);
            this.ws = socket;
            
            socket.onopen = () => {
                if (this.ws !== socket) return;
                console.log('WebSocket connected');
                this.updateConnectionStatus(true);
                this.reconnectAttempts = 0;
                this.refreshWebSocketWatchdog(socket);
            };

            socket.onmessage = (event) => {
                if (this.ws !== socket) return;
                this.refreshWebSocketWatchdog(socket);
                try {
                    this.handleMessage(JSON.parse(event.data));
                } catch (error) {
                    console.error('Invalid WebSocket message:', error);
                }
            };

            socket.onerror = (error) => {
                if (this.ws !== socket) return;
                console.error('WebSocket error:', error);
                this.updateConnectionStatus(false);
                socket.close();
            };

            socket.onclose = () => {
                if (this.ws !== socket) return;
                clearTimeout(this.wsMessageTimer);
                this.wsMessageTimer = null;
                this.ws = null;
                console.log('WebSocket disconnected');
                this.updateConnectionStatus(false);
                this.attemptReconnect();
            };
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.attemptReconnect();
        }
    }

    refreshWebSocketWatchdog(socket) {
        clearTimeout(this.wsMessageTimer);
        this.wsMessageTimer = setTimeout(() => {
            if (this.ws !== socket) return;

            console.warn('WebSocket timed out: no server messages received');
            this.ws = null;
            this.wsMessageTimer = null;
            this.updateConnectionStatus(false);
            socket.close();
            this.attemptReconnect();
        }, this.wsMessageTimeout);
    }

    attemptReconnect() {
        if (this.reconnectTimer !== null) {
            return;
        }

        const delay = Math.min(this.maxReconnectDelay, 500);
        this.reconnectAttempts++;
        console.log(`Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts})`);
        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            this.connectWebSocket();
        }, delay);
    }

    handleMessage(data) {
        if (data.type === 'status') {
            this.updateStatus(data);
        } else if (data.type === 'motor_configs' && Array.isArray(data.motor_configs)) {
            this.renderMotorControls(data.motor_configs);
        }
    }

    renderMotorControls(motorConfigs) {
        const container = document.getElementById('motorControls');
        container.replaceChildren();

        motorConfigs.forEach((motor, channel) => {
            const control = document.createElement('div');
            control.className = 'channel-control';

            const header = document.createElement('div');
            header.className = 'channel-header';

            const label = document.createElement('label');
            label.htmlFor = `motor-${channel}-slider`;
            label.textContent = motor.name || `Канал ${motor.id}`;

            const details = document.createElement('span');
            details.className = 'channel-pins';
            if (motor.type === 1) {
                details.textContent = `GPIO ${motor.forward_gpio} / ${motor.reverse_gpio}`;
            } else if (motor.type === 2) {
                details.textContent = `CAN 0x${Number(motor.can_id || 0).toString(16).toUpperCase()}`;
            } else {
                details.textContent = 'Damiao CAN';
            }
            header.append(label, details);

            const sliderContainer = document.createElement('div');
            sliderContainer.className = 'slider-container';

            const slider = document.createElement('input');
            slider.type = 'range';
            slider.id = `motor-${channel}-slider`;
            slider.min = '-100';
            slider.max = '100';
            slider.step = '1';
            slider.value = '0';
            slider.className = 'slider';

            const input = document.createElement('input');
            input.type = 'number';
            input.min = '-100';
            input.max = '100';
            input.step = '1';
            input.value = '0';
            input.className = 'number-input';
            input.setAttribute('aria-label', `${label.textContent} speed`);

            const unit = document.createElement('span');
            unit.className = 'unit';
            unit.textContent = '%';

            slider.addEventListener('input', () => {
                input.value = slider.value;
                this.setPwmChannel(channel, slider.value);
            });
            slider.addEventListener('change', () => {
                slider.value = '0';
                input.value = '0';
                this.setPwmChannel(channel, 0);
            });
            input.addEventListener('input', () => {
                const value = Number(input.value);
                if (Number.isInteger(value) && value >= -100 && value <= 100) {
                    slider.value = String(value);
                    this.setPwmChannel(channel, value);
                }
            });

            sliderContainer.append(slider, input, unit);
            control.append(header, sliderContainer);
            container.append(control);
        });
    }

    updateStatus(data) {
        document.getElementById('motorState').textContent = this.getStateName(data.state);
        document.getElementById('motorPosition').textContent = (data.position/Math.PI*180).toFixed(3);
        document.getElementById('motorVelocity').textContent = (data.velocity/Math.PI*180).toFixed(3);
        document.getElementById('motorTorque').textContent = data.torque.toFixed(3);
        document.getElementById('motorTMOS').textContent = data.t_mos.toFixed(1);
        document.getElementById('motorTRotor').textContent = data.t_rotor.toFixed(1);
    }

    getStateName(state) {
        const states = {
            0x0: 'Disabled',
            0x1: 'Enabled',
            0x8: 'Over-Voltage',
            0x9: 'Under-Voltage',
            0xA: 'Over-Current',
            0xB: 'MOS Over-Temp',
            0xC: 'Rotor Over-Temp',
            0xD: 'Lost Comm',
            0xE: 'Overload'
        };
        return states[state] || `Unknown (${state})`;
    }

    updateConnectionStatus(connected) {
        const indicator = document.getElementById('statusIndicator');
        const statusText = indicator.querySelector('.status-text');
        indicator.classList.toggle('connected', connected);
        indicator.classList.toggle('disconnected', !connected);
        if(statusText)
            if (connected) {
                statusText.textContent = 'WebSocket connected';
            } else {
                statusText.textContent = 'WebSocket disconnected; reconnecting';
            }
    }

    sendMessage(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        } else {
            console.warn('WebSocket not connected');
        }
    }

    enableMotor() {
        this.sendMessage({
            type: 'command',
            action: 'enable'
        });
    }

    disableMotor() {
        this.sendMessage({
            type: 'command',
            action: 'disable'
        });
    }

    setTorque(value) {
        const torque = parseFloat(value);
        if (!isNaN(torque) && torque >= -28 && torque <= 28) {
            this.sendMessage({
                type: 'control',
                action: 'torque',
                value: torque
            });
        }
    }

    setKp(value) {
        const kp = parseFloat(value);
        if (!isNaN(kp) && kp >= 0 && kp <= 500) {
            this.sendMessage({
                type: 'control',
                action: 'kp',
                value: kp
            });
        }
    }

    setKd(value) {
        const kd = parseFloat(value);
        if (!isNaN(kd) && kd >= 0 && kd <= 5) {
            this.sendMessage({
                type: 'control',
                action: 'kd',
                value: kd
            });
        }
    }

    setPwmChannel(channel, value) {
        const pwmValue = parseInt(value, 10);
        if (!isNaN(pwmValue) && pwmValue >= -100 && pwmValue <= 100) {
            this.sendMessage({
                type: 'pwm',
                channel: channel,
                value: pwmValue
            });
        }
    }

    stopAllPwm() {
        this.sendMessage({
            type: 'command',
            action: 'stop_all_pwm'
        });
    }

    clearError() {
        this.sendMessage({
            type: 'command',
            action: 'clear_error'
        });
    }

    setZeroPosition() {
        this.sendMessage({
            type: 'command',
            action: 'set_zero'
        });
    }

    setupEventListeners() {
        // Buttons
        document.getElementById('enableBtn').addEventListener('click', () => this.enableMotor());
        document.getElementById('disableBtn').addEventListener('click', () => this.disableMotor());
        document.getElementById('clearErrorBtn').addEventListener('click', () => this.clearError());
        document.getElementById('setZeroBtn').addEventListener('click', () => this.setZeroPosition());
        document.getElementById('stopAllPwmBtn').addEventListener('click', () => this.stopAllPwm());

        // Torque controls
        const torqueSlider = document.getElementById('torqueSlider');
        const torqueInput = document.getElementById('torqueInput');

        torqueSlider.addEventListener('input', (e) => {
            torqueInput.value = e.target.value;
            this.setTorque(e.target.value);
        });

        torqueSlider.addEventListener('change', (e) => {
            console.log('Torque slider changed:', e.target.value);
            torqueSlider.value = 0;
            torqueInput.value = 0;
            this.setTorque(e.target.value);
        });

        torqueInput.addEventListener('input', (e) => {
            torqueSlider.value = e.target.value;
            this.setTorque(e.target.value);
        });

        // Kp controls
        const kpSlider = document.getElementById('kpSlider');
        const kpInput = document.getElementById('kpInput');

        kpSlider.addEventListener('input', (e) => {
            kpInput.value = e.target.value;
            this.setKp(e.target.value);
        });

        kpInput.addEventListener('input', (e) => {
            kpSlider.value = e.target.value;
            this.setKp(e.target.value);
        });

        // Kd controls
        const kdSlider = document.getElementById('kdSlider');
        const kdInput = document.getElementById('kdInput');

        kdSlider.addEventListener('input', (e) => {
            kdInput.value = e.target.value;
            this.setKd(e.target.value);
        });

        kdInput.addEventListener('input', (e) => {
            kdSlider.value = e.target.value;
            this.setKd(e.target.value);
        });

    }
}

// Initialize controller when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.motorController = new MotorController();
});
