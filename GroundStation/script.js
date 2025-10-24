// --- Updated Data Processing & Display ---
function processThrustData(dataLine) {
    // Expected Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z"
    const parts = dataLine.split(',');
    let thrustValue = null;
    let tempValue = null;
    let humiValue = null;

    parts.forEach(part => {
        if (part.startsWith("Thrust:")) {
            thrustValue = parseFloat(part.replace('Thrust:', '').trim());
        } else if (part.startsWith("Temp:")) {
            tempValue = parseFloat(part.replace('Temp:', '').trim());
        } else if (part.startsWith("Humi:")) {
            humiValue = parseFloat(part.replace('Humi:', '').trim());
        }
    });

    if (thrustValue !== null) {
        // (Graph Update Logic for Thrust remains here)
        // ... (existing chart update for thrustData and timeLabels)
    }

    if (tempValue !== null && humiValue !== null) {
        // **NEW UI UPDATE:** Update status window with Temp/Humi readings
        statusDisplay.innerHTML = `
            STATUS: CONNECTED<br>
            **TEMP:** ${tempValue}°C | **HUMI:** ${humiValue}%
        `;
    }
}
