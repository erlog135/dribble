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

  debugLog('Inserting pin in the future: ' + JSON.stringify(pin));

  // Push the pin
  timeline.insertUserPin(pin, function (responseText) {
    debugLog('Result: ' + responseText);
  });
}

//TODO: maybe add support for weather alerts

// Helper to check if condition code represents rain or snow
function isPrecipitationCondition(conditionCode) {
  // 3: Rain, 4: Snow, 5: Thunder, 6: Mixed, 10: Hail
  return conditionCode === 3 ||
         conditionCode === 4 ||
         conditionCode === 5 ||
         conditionCode === 6 ||
         conditionCode === 10;
}

/**
 * Create a timeline pin for next hour precipitation
 * @param {Date} startTime - Start time of the precipitation event
 * @param {Date} endTime - End time of the precipitation event
 * @param {string} precipitationType - Type of precipitation (e.g., "Snow", "Rain", "Sleet")
 * @param {number} averageIntensityMmHr - Average precipitation intensity in mm/hr
 * @param {Array|Object} [forecastHours] - Array of 10-byte packed hour packages or options object
 * @param {number} [hourInterval] - Interval between forecast hours in hours (default 1 or 2)
 */
function createNextHourPrecipitationPin(startTime, endTime, precipitationType, averageIntensityMmHr, forecastHours, hourInterval) {

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
  if (precipitationType === "Snow" || precipitationType === "Sleet") {
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

  // If continuous precipitation continues past the 0th forecast hour, add continuation sentence
  var hours = Array.isArray(forecastHours) ? forecastHours : (forecastHours && forecastHours.forecastHours);
  var interval = (typeof hourInterval === "number" && hourInterval > 0)
    ? hourInterval
    : (forecastHours && typeof forecastHours.hourInterval === "number" ? forecastHours.hourInterval : 2);

  if (hours && hours.length > 1) {
    var hour1Condition = (hours[1][9] >> 4) & 0x0F;
    if (isPrecipitationCondition(hour1Condition)) {
      var stopIdx = 2;
      while (stopIdx < hours.length) {
        var cond = (hours[stopIdx][9] >> 4) & 0x0F;
        if (!isPrecipitationCondition(cond)) {
          break;
        }
        stopIdx++;
      }

      var relativeHour;
      if (stopIdx < hours.length) {
        relativeHour = hours[stopIdx][0];
      } else {
        var lastRelativeHour = hours[hours.length - 1][0];
        relativeHour = lastRelativeHour + interval;
      }

      var stopDate = new Date(startTime.getTime() + relativeHour * 3600000);
      var stopHour = stopDate.getHours();
      var ampm = stopHour >= 12 ? "PM" : "AM";
      var h12 = stopHour % 12;
      if (h12 === 0) h12 = 12;
      var hh = h12 + ampm;

      var precipWord = (precipitationType === "Snow" || precipitationType === "Sleet") ? "Snow" : "Rain";
      bodyText += " " + precipWord + " should continue until " + hh + ".";
    }
  }

  // Determine icon based on precipitation type and intensity
  var iconName = "LIGHT_RAIN"; // default
  if (precipitationType === "Rain") {
    if (averageIntensityMmHr > 7.6) {
      iconName = "HEAVY_RAIN";
    } else {
      iconName = "LIGHT_RAIN";
    }
  } else if (precipitationType === "Snow" || precipitationType === "Sleet") {
    if (averageIntensityMmHr > 2.5) {
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
    },
    "actions": [
      {
        "title": "View Forecast",
        "type": "openWatchApp",
        "launchCode": 0
      }
    ]
  };

  debugLog('Creating precipitation pin: ' + JSON.stringify(pin));

  return pin;
}


module.exports.pushTestPin = pushTestPin;
module.exports.createNextHourPrecipitationPin = createNextHourPrecipitationPin;