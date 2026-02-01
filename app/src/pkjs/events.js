/**
 * events.js
 * 
 * This file is responsible for interacting with the Pebble Timeline API
 * and managing events or actions that occur outside the main app,
 * such as pushing pins to the timeline or handling background events.
 */

var timeline = require('./timeline');

// Debug configuration
var debug = false;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}

// Push a pin when the app starts
function pushTestPin() {
  // An hour ahead
  var date = new Date();
  date.setHours(date.getHours() + 1);

  // Create the pin
  var pin = {
    "id": "00-00-next-hour-precipitation",
    "time": date.toISOString(),
    "duration": 32,
   "layout": {
      "type": "calendarPin",
      "backgroundColor": "#00AAFF",
      "title": "Rain",
      "locationName": "for 32m",
      "body": "Steady precipitation coming down. Expect <amt> over <dur>.",
      "tinyIcon": "app://images/RAINDROP",
      "largeIcon": "app://images/RAINDROP",
      "lastUpdated": new Date().toISOString()
    }
  };

  // Construct the app glance slice object
  var appGlanceSlices = [];


  // Trigger a reload of the slices in the app glance
  //TODO: add this back in
  // app glance is doing weird stuff to the app
  Pebble.appGlanceReload(appGlanceSlices, appGlanceSuccess, appGlanceFailure);

  debugLog('Inserting pin in the future: ' + JSON.stringify(pin));

  // Push the pin
  timeline.insertUserPin(pin, function(responseText) {
    debugLog('Result: ' + responseText);
  });
}

//TODO: maybe add support for weather alerts

/**
 * Create a timeline pin for next hour precipitation
 * @param {Date} startTime - Start time of the precipitation event
 * @param {Date} endTime - End time of the precipitation event
 * @param {string} precipitationType - Type of precipitation (e.g., "Snow", "Rain", "Sleet")
 * @param {number} averageIntensityMmHr - Average precipitation intensity in mm/hr
 */
function createNextHourPrecipitationPin(startTime, endTime, precipitationType, averageIntensityMmHr) {

  if (averageIntensityMmHr < 0.1) {
    return null;
  }

  var intensityName = "";

  if (precipitationType === "Rain") {
    if (averageIntensityMmHr >= 2.5 && averageIntensityMmHr <= 7.6) {
      intensityName = "Moderate";
    } else if (averageIntensityMmHr > 7.6) {
      intensityName = "Heavy";
    } else {
      intensityName = "Light";
    }
  } else if (precipitationType === "Snow" || precipitationType === "Sleet") {
    if (averageIntensityMmHr >= 1.0 && averageIntensityMmHr <= 2.5) {
      intensityName = "Moderate";
    } else if (averageIntensityMmHr > 2.5) {
      intensityName = "Heavy";
    } else {
      intensityName = "Light";
    }
  }


  // Calculate duration in minutes
  var durationMs = endTime.getTime() - startTime.getTime();
  var durationMinutes = Math.round(durationMs / (1000 * 60));
  
  // Format duration for display
  var durationText = durationMinutes >= 60 ? 
    Math.floor(durationMinutes / 60) + 'h ' + (durationMinutes % 60) + 'm' : 
    durationMinutes + 'm';
  
  // Calculate total amount from average intensity (mm/hr * hours)
  var durationHours = durationMinutes / 60;
  var amountMm = averageIntensityMmHr * durationHours;

  // Get precipitation unit from clay settings (default: in, per config.js)
  var precipUnit = "in";
  if (typeof localStorage !== "undefined" && localStorage.getItem("clay-settings")) {
    var settings = JSON.parse(localStorage.getItem("clay-settings"));
    precipUnit = settings.CFG_PRECIPITATION_UNITS || "in";
  }

  //if snow or sleet, multiply expected amount by 10 to get a more accurate accumulated amount
  if(precipitationType === "Snow" || precipitationType === "Sleet") {
    amountMm = (amountMm * 10);
  }

  var expectedAmount;
  if (precipUnit === "in") {
    expectedAmount = (amountMm * 0.0393701).toFixed(2) + "in";
  } else {
    expectedAmount = amountMm.toFixed(1) + "mm";
  }


  // Create generic body text
  var bodyText = "Expect about " + expectedAmount + " over " + durationText + ".";
  
  // Determine icon based on precipitation type and intensity
  var iconName = "LIGHT_RAIN"; // default
  if (precipitationType === "Rain") {
    if(averageIntensityMmHr > 7.6) {
      iconName = "HEAVY_RAIN";
    } else {
      iconName = "LIGHT_RAIN";
    }
  } else if (precipitationType === "Snow" || precipitationType === "Sleet") {
    if(averageIntensityMmHr > 2.5) {
      iconName = "HEAVY_SNOW";
    } else {
      iconName = "LIGHT_SNOW";
    }
  }


  

  // Create the pin
  var pin = {
    "id": "next_hour_precipitation",
    "time": startTime.toISOString(),
    "duration": durationMinutes,
    "layout": {
      "type": "calendarPin",
      "backgroundColor": "#00AAFF",
      "title": intensityName + (intensityName !== "" ? " " : "") + precipitationType,
      "locationName": "for " + durationText,
      "body": bodyText,
      "tinyIcon": "system://images/" + iconName,
      "largeIcon": "system://images/" + iconName,
      "lastUpdated": new Date().toISOString()
    }
  };

  debugLog('Creating precipitation pin: ' + JSON.stringify(pin));

  return pin;
}


module.exports.pushTestPin = pushTestPin;
module.exports.createNextHourPrecipitationPin = createNextHourPrecipitationPin;