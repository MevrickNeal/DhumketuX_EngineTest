let apiForm = document.getElementById("apiForm");

apiForm?.addEventListener("submit", function (event) {
  event.preventDefault(); // Prevent form submission's default action

  const apiInput = document.getElementById("apiInput").value;

  // Check if the input is not empty
  if (apiInput.trim() !== "") {
    // Save the API to local storage
    localStorage.setItem("api", apiInput);

    // Close the modal after saving
    const modal = document.getElementById("modal");
    modal.style.display = "none";

    // You can perform other actions after saving the API, like reloading the page or fetching data using the API
  } else {
    alert("Please enter a valid API!");
  }
});

async function getApiData() {
  let api = localStorage.getItem("api");
  let url = `${api}/dx-kinetics`;
  // let url = 'http://192.168.0.111:3210/dx-kinetics';
  try {
    let res = await fetch(url);
    return await res.json();
  } catch (error) {
    console.log(error);
  }
}

// Render Data
async function renderData() {
  let database = await getApiData();
  console.log(database);
  //   console.log(typeof database.data.altitude);
  if (database.isSuccess) {
    let altitudeValue;
    database.data.altitude < 10
      ? (altitudeValue = `0${database.data.altitude}`)
      : (altitudeValue = database.data.altitude);

    let accelerationValue;
    database.data.acceleration < 10
      ? (accelerationValue = `0${database.data.acceleration}`)
      : (accelerationValue = database.data.acceleration);
    let setRangeValue = database.data.setRange;
    let currentRangeValue = database.data.currentRange;
    let velocityValue;
    database.data.velocity < 10
      ? (velocityValue = `0${database.data.velocity}`)
      : (velocityValue = database.data.velocity);

    // update ui
    let altitude = document.querySelector(".altitudeValue");
    let acceleration = document.querySelector(".accelerationValue");
    let velocity = document.querySelector(".velocityValue");

    altitude.textContent = "Altitude " + altitudeValue + " ft";
    acceleration.textContent = accelerationValue;
    velocity.textContent = velocityValue;

    const progress = document.querySelector(".progress-done");
    const percent = document.querySelector("small");

    let percentage = (currentRangeValue / setRangeValue) * 100;
    progress.style.height = percentage + "%";
    percent.textContent = `${parseInt(percentage)}%`;
  }
}

setInterval("renderData()", 1000); // Call a function every 10000 milliseconds (OR 10 seconds).
