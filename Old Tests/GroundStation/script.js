// --- GLOBAL VARIABLES ---
let port; // The serial port object
let reader; // The stream reader for receiving data
let outputStream; // The stream writer for sending data
let chart;

// Data storage for charting and CSV export
let telemetryData = []; // Stores { time: timeString, thrust: N, temp: C, humi: RH }
let timeLabels = [];
let thrustValues = [];

// DOM Elements
const connectBtn = document.getElementById('connectBtn');
const saveDataBtn = document.getElementById('saveDataBtn');
const statusDisplayBox = document.getElementById('status-display-box');
const armBtn = document.getElementById('armBtn');
const safeBtn = document.getElementById('safeBtn');
const testBtn = document.getElementById('testBtn');
const launchBtn = document.getElementById('launchBtn');
const webcamFeed = document.getElementById('webcam-feed');

// Command Protocol Mapping (matches Launch Pad Arduino)
const COMMANDS = {
    ARM: 'A',
    SAFE: 'S',
    TEST: 'T',
    LAUNCH: 'I'
};

// --- INITIALIZATION ---
window.onload = init;

function init() {
    initChart();
    initWebcam();
    initEventListeners();
}

function initEventListeners() {
    connectBtn.addEventListener('click', connectSerial);
    saveDataBtn.addEventListener('click', saveCSV);
    armBtn.addEventListener('click', () => sendCommand(COMMANDS.ARM));
    safeBtn.addEventListener('click', () => sendCommand(COMMANDS.SAFE));
    testBtn.addEventListener('click', () => sendCommand(COMMANDS.TEST));
    launchBtn.addEventListener('click', () => sendCommand(COMMANDS.LAUNCH));
}


// --- 1. WEB SERIAL API CONNECTION ---

async function connectSerial() {
    // Check for Web Serial API support
    if (!('serial' in navigator)) {
        statusDisplayBox.innerHTML = 'STATUS: **ERROR** - Web Serial API not supported in this browser.';
        return;
    }

    try {
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 9600 }); 

        statusDisplayBox.innerHTML = 'STATUS: **CONNECTING...**';
        connectBtn.disabled = true;

        // Enable command buttons
        armBtn.disabled = false;
        safeBtn.disabled = false;
        testBtn.disabled = false;
        launchBtn.disabled = false;
        
        // Setup streams for reading (receiving data)
        const textDecoder = new TextDecoderStream();
        port.readable.pipeTo(textDecoder.writable);
        reader = textDecoder.readable.getReader();

        // Setup streams for writing (sending commands)
        const textEncoder = new TextEncoderStream();
        textEncoder.readable.pipeTo(port.writable);
        outputStream = textEncoder.writable;

        // Start the reading loop
        readLoop();

    } catch (error) {
        console.error('Serial Connection Error:', error);
        statusDisplayBox.innerHTML = `STATUS: **ERROR** - ${error.message}`;
        connectBtn.disabled = false;
    }
}

async function readLoop() {
    while (true) {
        try {
            const { value, done } = await reader.read();
            if (done) {
                console.log('Reader has been closed.');
                break;
            }
            
            if (value) {
                // Split incoming data by newline and process the last non-empty line
                const lines = value.split('\n').filter(line => line.trim().length > 0);
                if (lines.length > 0) {
                    processTelemetry(lines.pop().trim());
                }
            }
        } catch (error) {
            console.error('Error reading serial data:', error);
            // Optionally close the port here
            break;
        }
    }
}

// --- 2. DATA PROCESSING & CHARTING ---

function processTelemetry(dataLine) {
    // Expected Format from Arduino: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z"
    const parts = dataLine.split(',');
    let dataPoint = { thrust: null, temp: null, humi: null };

    parts.forEach(part => {
        if (part.startsWith("Thrust:")) {
            dataPoint.thrust = parseFloat(part.replace('Thrust:', '').trim());
        } else if (part.startsWith("Temp:")) {
            dataPoint.temp = parseFloat(part.replace('Temp:', '').trim());
        } else if (part.startsWith("Humi:")) {
            dataPoint.humi = parseFloat(part.replace('Humi:', '').trim());
        }
    });

    if (dataPoint.thrust !== null) {
        const now = new Date();
        const timeStamp = now.toLocaleTimeString('en-US');

        // Store data for CSV export
        telemetryData.push({ 
            time: timeStamp, 
            thrust: dataPoint.thrust, 
            temp: dataPoint.temp, 
            humi: dataPoint.humi 
        });

        // Update Chart data arrays
        timeLabels.push(timeStamp);
        thrustValues.push(dataPoint.thrust);

        // Keep the last 100 points visible on the chart for performance
        const MAX_POINTS = 100;
        if (timeLabels.length > MAX_POINTS) {
            timeLabels.shift();
            thrustValues.shift();
        }

        // Update the Chart
        chart.data.labels = timeLabels;
        chart.data.datasets[0].data = thrustValues;
        chart.update('none'); 
    }

    // Update Status Display with the latest environmental data
    if (dataPoint.temp !== null && dataPoint.humi !== null) {
        statusDisplayBox.innerHTML = `
            STATUS: **ONLINE**
            THRUST: ${dataPoint.thrust.toFixed(2)} N
            TEMP: ${dataPoint.temp.toFixed(1)} °C
            HUMI: ${dataPoint.humi.toFixed(1)} %
        `;
    }
}

function initChart() {
    const ctx = document.getElementById('thrustChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeLabels,
            datasets: [{
                label: 'Real-Time Thrust (N)',
                data: thrustValues,
                borderColor: 'rgba(75, 192, 192, 0.8)',
                borderWidth: 2,
                pointRadius: 0, // Hide data points
                fill: false,
                tension: 0.1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: true,
                    title: { display: true, text: 'Thrust (N)', color: '#e0e0e0' },
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    ticks: { color: '#e0e0e0' }
                },
                x: {
                    title: { display: true, text: 'Time', color: '#e0e0e0' },
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    ticks: { color: '#e0e0e0' }
                }
            },
            plugins: {
                legend: { display: false },
                tooltip: { mode: 'index', intersect: false }
            }
        }
    });
}

// --- 3. COMMAND TRANSMISSION ---

async function sendCommand(command) {
    if (!port || !outputStream) {
        statusDisplayBox.innerHTML = "STATUS: **ERROR** - Not connected. Click CONNECT.";
        return;
    }

    try {
        const writer = outputStream.getWriter();
        // Send the command character followed by a newline (optional, but good practice)
        await writer.write(command + '\n');
        writer.releaseLock();
        
        statusDisplayBox.innerHTML += `\n**COMMAND SENT:** ${command}`;

        // Special handling for the LAUNCH command (30-second delay could be implemented here)
        if (command === COMMANDS.LAUNCH) {
            // Placeholder for full countdown logic
            statusDisplayBox.innerHTML += '\n**LAUNCH SEQUENCE STARTED!**';
        }

    } catch (error) {
        console.error('Error sending command:', error);
        statusDisplayBox.innerHTML = `STATUS: **ERROR** - Command send failed: ${error.message}`;
    }
}

// --- 4. UTILITIES ---

function initWebcam() {
    // Requires HTTPS or localhost to access the webcam
    if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
        navigator.mediaDevices.getUserMedia({ video: true })
            .then(stream => {
                webcamFeed.srcObject = stream;
            })
            .catch(err => {
                console.error("Webcam access denied or failed:", err);
            });
    }
}

function saveCSV() {
    if (telemetryData.length === 0) {
        alert("No data collected yet!");
        return;
    }

    // Create CSV header
    let csv = "Time,Thrust (N),Temperature (°C),Humidity (%)\n";

    // Add data rows
    telemetryData.forEach(row => {
        csv += `${row.time},${row.thrust.toFixed(2)},${row.temp.toFixed(1)},${row.humi.toFixed(1)}\n`;
    });

    // Use FileSaver.js to download the file
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
    saveAs(blob, `DhumketuX_Test_Data_${new Date().toISOString().replace(/[:.]/g, "-")}.csv`);
    
    statusDisplayBox.innerHTML += "\n**DATA SAVED** as CSV.";
}
