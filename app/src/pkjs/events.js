/**
 * events.js
 * 
 * This file is responsible for interacting with the Pebble Timeline API
 * and managing events or actions that occur outside the main app,
 * such as pushing pins to the timeline or handling background events.
 */

var timeline = require('./timeline');
var precipitationTemplates = require('./templates/precipitation.json');

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
 * @param {number} intensity - Precipitation intensity in mm/hr
 */
function createNextHourPrecipitationPin(startTime, endTime, precipitationType, intensity) {

  if (intensity < 0.1) {
    return null;
  }

  // Calculate duration in minutes
  var durationMs = endTime.getTime() - startTime.getTime();
  var durationMinutes = Math.round(durationMs / (1000 * 60));
  
  // Format duration for display
  var durationText = durationMinutes >= 60 ? 
    Math.floor(durationMinutes / 60) + 'h ' + (durationMinutes % 60) + 'm' : 
    durationMinutes + 'm';
  
  // Calculate expected precipitation amount
  var durationHours = durationMinutes / 60;
  var expectedAmount = (intensity * durationHours).toFixed(1);
  
  // Determine intensity category based on thresholds
  var intensityCategory = "light";
  if (intensity >= 7.5) {
    intensityCategory = "heavy";
  } else if (intensity >= 2.5) {
    intensityCategory = "moderate";
  }
  
  // Map precipitation type to template key
  var templateKey = "generic"; // default
  var typeLower = precipitationType.toLowerCase();
  if (typeLower.includes("rain")) {
    templateKey = "rain";
  } else if (typeLower.includes("snow")) {
    templateKey = "snow";
  } else if (typeLower.includes("thunder")) {
    templateKey = "thunder";
  } else if (typeLower.includes("hail")) {
    templateKey = "hail";
  }
  
  // Get templates for this type and intensity
  var templates = precipitationTemplates[templateKey][intensityCategory].templates;
  
  // Select a random template
  var randomIndex = Math.floor(Math.random() * templates.length);
  var selectedTemplate = templates[randomIndex];
  
  // Replace placeholders in template
  var bodyText = selectedTemplate
    .replace(/<amt>/g, expectedAmount + " mm")
    .replace(/<dur>/g, durationText);
  
  // Determine icon based on precipitation type
  var iconName = "RAINDROP"; // default
  if (typeLower.includes("rain")) {
    iconName = "RAINDROP";
  } else if (typeLower.includes("snow")) {
    iconName = "SNOWFLAKE";
  }

  // Format date for pin ID (mm-dd)
  var month = (startTime.getMonth() + 1).toString().padStart(2, '0');
  var day = startTime.getDate().toString().padStart(2, '0');
  var dateString = month + '-' + day;

  // Create the pin
  var pin = {
    "id": dateString + "-next-hour-precipitation",
    "time": startTime.toISOString(),
    "duration": durationMinutes,
    "layout": {
      "type": "calendarPin",
      "backgroundColor": "#00AAFF",
      "title": precipitationType,
      "locationName": "for " + durationText,
      "body": bodyText,
      "tinyIcon": "app://images/" + iconName,
      "largeIcon": "app://images/" + iconName,
      "lastUpdated": new Date().toISOString()
    }
  };

  debugLog('Creating precipitation pin: ' + JSON.stringify(pin));

  return pin;
}

/**
 * Create a timeline pin for precipitation chance
 * @param {Date} startTime - Start time of the precipitation chance period
 * @param {string} precipitationType - Type of precipitation (e.g., "Snow", "Rain", "Sleet")
 * @param {number} chance - Precipitation chance as a percentage (0-100)
 */
function createPrecipitationChancePin(startTime, precipitationType, chance) {

  if (chance <= 0) {
    return null;
  }

  // Format date for pin ID (mm-dd)
  var month = (startTime.getMonth() + 1).toString().padStart(2, '0');
  var day = startTime.getDate().toString().padStart(2, '0');
  var dateString = month + '-' + day;

  // Determine icon based on precipitation type
  var iconName = "RAINDROP"; // default
  var typeLower = precipitationType.toLowerCase();
  if (typeLower.includes("rain")) {
    iconName = "RAINDROP";
  } else if (typeLower.includes("snow")) {
    iconName = "SNOWFLAKE";
  }

  // Create the pin
  var pin = {
    "id": dateString + "-chance-precipitation",
    "time": startTime.toISOString(),
    "layout": {
      "type": "genericPin",
      "backgroundColor": "#00AAFF",
      "title": precipitationType + " Chance",
      "body": "There is a " + chance + "% chance of " + precipitationType.toLowerCase() + " starting at this time.",
      "tinyIcon": "app://images/" + iconName,
      "lastUpdated": new Date().toISOString()
    }
  };

  debugLog('Creating precipitation chance pin: ' + JSON.stringify(pin));

  return pin;
}



function appGlanceSuccess(appGlanceSlices, appGlanceReloadResult) {
  debugLog('SUCCESS!');
};

function appGlanceFailure(appGlanceSlices, appGlanceReloadResult) {
  debugLog('FAILURE!');
};


module.exports.pushTestPin = pushTestPin;
module.exports.createNextHourPrecipitationPin = createNextHourPrecipitationPin;
module.exports.createPrecipitationChancePin = createPrecipitationChancePin;