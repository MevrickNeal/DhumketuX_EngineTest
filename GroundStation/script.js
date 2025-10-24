// Global variables
let port;
let reader;
let thrustData = [];
let timeLabels = [];
let chart;

const connectBtn = document.getElementById('connectBtn');
const statusDisplay = document.getElementById('status-display');

// --- 1. WEB SERIAL API CONNECTION ---
async function connectSerial() {
    try {
        // Request the serial port
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 9600 }); // Must match Arduino's Serial.begin()

        statusDisplay.textContent = "STATUS: CONNECTED";
        statusDisplay.style.color = '#4caf50'; // Green

        // Disable connect and enable controls
        connectBtn.disabled = true;
        document.getElementById('launchBtn').disabled = false;
        document.getElementById('testBtn').disabled = false;

        // Begin reading data
        readData();

    } catch (error) {
        console.error('Serial connection failed:', error);
        statusDisplay.textContent = "STATUS: CONNECTION FAILED";
        statusDisplay.style.color = '#f44336'; // Red
    }
}

// --- 2. DATA READING LOOP ---
async function readData() {
    reader = port.readable.getReader();
    const decoder = new TextDecoder();

    try {
        while (true) {
            const { value, done } = await reader.read();
            if (done) {
                // Reader closed, exit loop
                break;
            }
            // value is a Uint8Array, decode it to a string
            const dataString = decoder.decode(value);
            
            // Arduino sends data line by line, so we split it
            dataString.split('\n').forEach(line => {
                if (line.trim().length > 0) {
                    processThrustData(line.trim());
                }
            });
        }
    } catch (error) {
        console.error('Error reading serial data:', error);
    } finally {
        reader.releaseLock();
    }
}

// --- 3. DATA PROCESSING & GRAPH UPDATE ---
function processThrustData(dataLine) {
    // Example format expected from Arduino (after LoRa relay): "Thrust: 15.42 N"
    if (dataLine.startsWith("Thrust:")) {
        const thrustValue = parseFloat(dataLine.replace('Thrust:', '').replace('N', '').trim());
        
        // Push new data point and timestamp
        const now = new Date();
        const timeStamp = now.toLocaleTimeString('en-US', { second: '2-digit' });
        
        // Cap the number of data points for performance
        const MAX_POINTS = 100; 
        if (thrustData.length >= MAX_POINTS) {
            thrustData.shift();
            timeLabels.shift();
        }

        thrustData.push(thrustValue);
        timeLabels.push(timeStamp);

        // Update the Chart
        if (chart) {
            chart.data.labels = timeLabels;
            chart.data.datasets[0].data = thrustData;
            chart.update('none'); // 'none' for instant update
        }
    }
}

// --- 4. INITIALIZE CHART, WEBCAM, & EVENTS ---
function init() {
    // A. Initialize Chart.js
    const ctx = document.getElementById('thrustChart').getContext('2d');
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: timeLabels,
            datasets: [{
                label: 'Real-Time Thrust (N)',
                data: thrustData,
                borderColor: 'rgba(75, 192, 192, 0.8)',
                tension: 0.1
            }]
        },
        options: {
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
                legend: { display: false }
            },
            // Glassy look for the chart area
            backgroundColor: 'rgba(255, 255, 255, 0.05)' 
        }
    });

    // B. Initialize Webcam (requires HTTPS or localhost)
    if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
        navigator.mediaDevices.getUserMedia({ video: true })
            .then(stream => {
                document.getElementById('webcam-feed').srcObject = stream;
            })
            .catch(err => {
                console.error("Webcam access denied or failed:", err);
            });
    }

    // C. Event Listeners
    connectBtn.addEventListener('click', connectSerial);
    document.getElementById('saveDataBtn').addEventListener('click', saveCSV);
    // You'll add listeners for launchBtn and testBtn here later.
}

// D. CSV Save Function (Placeholder for now)
function saveCSV() {
    console.log("Saving data...");
    // Logic to build CSV string from timeLabels and thrustData
    // e.g., "Time,Thrust\n12:00:01,0.0\n12:00:02,15.42..."
    // and then create a Blob and download it.
}

window.onload = init;
