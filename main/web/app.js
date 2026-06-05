class MotorController {
    constructor() {
        this.ws = null;
        this.wsUrl =`ws://${window.location.protocol.startsWith("http")
    ? window.location.hostname : "192.168.1.7"}/ws`;
//  `ws://${window.location.host}/ws`;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 3000;
        
        this.init();
    }

    init() {
        this.connectWebSocket();
        this.setupEventListeners();
    }

    connectWebSocket() {
        try {
            this.ws = new WebSocket(this.wsUrl);
            
            this.ws.onopen = () => {
                console.log('WebSocket connected');
                this.updateConnectionStatus(true);
                this.reconnectAttempts = 0;
            };

            this.ws.onmessage = (event) => {
                this.handleMessage(JSON.parse(event.data));
            };

            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.updateConnectionStatus(false);
            };

            this.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.updateConnectionStatus(false);
                this.attemptReconnect();
            };
        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.attemptReconnect();
        }
    }

    attemptReconnect() {
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`Reconnecting... attempt ${this.reconnectAttempts}`);
            setTimeout(() => this.connectWebSocket(), this.reconnectDelay);
        } else {
            console.error('Max reconnection attempts reached');
        }
    }

    handleMessage(data) {
        if (data.type === 'status') {
            this.updateStatus(data);
        }
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
        
        if (connected) {
            indicator.classList.add('connected');
            statusText.textContent = 'Connected';
        } else {
            indicator.classList.remove('connected');
            statusText.textContent = 'Disconnected';
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
