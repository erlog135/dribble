var keys = require('message_keys');
var msgproc = require('./msgproc');
var weatherkit = require('./weatherkit');
var events = require('./events');
var timeline = require('./timeline');

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

var API_KEY = require('./api_keys').API_KEY;

var method = "GET";
var availabilityURL = "https://weatherkit.apple.com/api/v1/availability/";
var dataURL = "https://weatherkit.apple.com/api/v1/weather/";

var language = "";

var forecastHours = [];
var precipitation = null;

var MAX_HOURS = 12;
var hourInterval = 2; // Default to 2 hours
var refreshInterval = 30; // Default to 30 minutes

var dummyMode = false;

// Debug configuration
var debug = false;
var checkCache = true;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}


Pebble.addEventListener("ready",
    function (e) {
        debugLog("PebbleKit JS Ready - Starting automatic weather fetch");
        language = Pebble.getActiveWatchInfo().language;


        if(localStorage.getItem('clay-settings')) {
            //debugLog((localStorage.getItem('clay-settings')));
            var settings = JSON.parse(localStorage.getItem('clay-settings'));
            hourInterval = parseInt(settings.CFG_DISPLAY_INTERVAL, 10) || 2;
            refreshInterval = parseInt(settings.CFG_REFRESH_INTERVAL, 10) || 30;

        }

        // Send JSReady signal to C app
        debugLog("Sending JSReady signal to C app");
        Pebble.sendAppMessage({"JS_READY": 1}, function() {
            debugLog("JSReady signal sent successfully");
            // Now start fetching weather data (check cache first)
            if(checkCache) {
                checkCacheOrFetchWeather();
            } else {
                getLocation();
            }
        }, function(e) {
            debugLog("JSReady signal failed: " + JSON.stringify(e));
            // Still try to fetch data even if JSReady failed
            if(checkCache) {
                checkCacheOrFetchWeather();
            } else {
                getLocation();
            }
        });
    }
);



// No AppMessage listener needed - C app only receives data, never sends requests

//code 0 for success
//1: hourly forecast failed
//2: location error
function sendResponseData(responseCode) {
    Pebble.sendAppMessage({ "RESPONSE_DATA": responseCode }, function () {
        debugLog("Response data sent, expecting hour requests");
    }, function (e) {
        debugLog("Response data failed: " + JSON.stringify(e));
    });
}

// Helper: build an array of given length filled with a value (ES5 replacement for Array.fill)
function filledArray(length, value) {
    var arr = [];
    for (var i = 0; i < length; i++) {
        arr.push(value);
    }
    return arr;
}

// Check cache first, then fetch fresh data if needed
function checkCacheOrFetchWeather() {
    debugLog("Checking cached weather data...");

    // Try to get cached weather data
    var cachedData = msgproc.getCachedWeatherData(refreshInterval);

    if (cachedData) {
        debugLog("Using cached weather data");

        // Use cached data
        forecastHours = cachedData.hourPackages;
        precipitation = cachedData.precipitationData;

        // Send cached data immediately
        if (forecastHours && forecastHours.length > 0) {
            // Ensure we have precipitation data (use empty if none cached)
            if (precipitation === null) {
                precipitation = msgproc.packPrecipitation(0, filledArray(24, 0));
            }
            sendAllWeatherData();
        } else {
            debugLog("Cached data invalid, fetching fresh data");
            getLocation();
        }
    } else {
        debugLog("No valid cached data found, fetching fresh weather data");
        getLocation();
    }
}

// Get current location using geolocation
function getLocation() {
    debugLog("Getting current location...");

    if (navigator.geolocation) {
        navigator.geolocation.getCurrentPosition(
            function(position) {
                debugLog("Location obtained successfully");
                var latitude = position.coords.latitude;
                var longitude = position.coords.longitude;
                var location = latitude + "/" + longitude;
                debugLog("Location: " + location);

                // Now request weather availability with the obtained location
                requestWeatherAvailability(location);

            },
            function(error) {
                debugLog("Error getting location: " + error.message);
                // Fallback to a default location or show error
                sendResponseData(2);
            },
            {
                enableHighAccuracy: true,
                timeout: 10000,
                maximumAge: 60000
            }
        );
    } else {
        debugLog("Geolocation not supported");
        // Fallback to a default location or show error
        sendResponseData(2);
    }
}



function sendAllWeatherData() {
    debugLog('Starting weather data transmission');

    if (forecastHours.length > 0) {
        debugLog('Packing ' + forecastHours.length + ' forecast hours into single 120-byte message');

        // Pack all hours into a single 120-byte array
        var allHourData = msgproc.packAllHourData(forecastHours);

        // Send all hour data in a single message
        Pebble.sendAppMessage({ "HOUR_DATA": allHourData }, function() {
            debugLog('All hourly data sent successfully! Sending precipitation...');
            if(precipitation != null) {
                debugLog('Sending precipitation data');
                sendPrecipitation();
            } else {
                debugLog('No precipitation data available, sending empty precipitation data');
                // Send empty precipitation data (type 0, all intensities 0)
                var emptyPrecipitation = msgproc.packPrecipitation(0, filledArray(24, 0));
                sendPrecipitationFromData(emptyPrecipitation);
            }
        }, function(e) {
            debugLog('Hour data transmission failed: ' + JSON.stringify(e));
        });
    } else {
        debugLog('No forecast hours to send, sending error response');
        sendResponseData(1); // Send error if no data
    }
}

function sendPrecipitation() {
    Pebble.sendAppMessage({ "PRECIPITATION_PACKAGE": precipitation }, function () {
        debugLog("Precipitation sent successfully!");
    }, function (e) {
        debugLog("Precipitation failed: " + JSON.stringify(e));
    });
}

function sendPrecipitationFromData(precipitationData) {
    Pebble.sendAppMessage({ "PRECIPITATION_PACKAGE": precipitationData }, function () {
        debugLog("Empty precipitation data sent successfully!");
    }, function (e) {
        debugLog("Empty precipitation data failed: " + JSON.stringify(e));
    });
}

function celsiusToFahrenheit(celsius) {
    return (celsius * 9/5) + 32;
}

//if the availability check is successful, request the actual weather data
function requestWeatherData(location, dataSets) {

    forecastHours = [];
    precipitation = null;

    debugLog("Requesting weather data");
    var url = dataURL + "en_US/" + location + "?dataSets=" + dataSets;
    var xhr = new XMLHttpRequest();

    xhr.onload = function () {
        if (xhr.status === 200) {
            debugLog("Weather data responded");
            //debugLog("Weather data: " + xhr.responseText);
            var response = JSON.parse(xhr.responseText);


            forecastHours = [];

            if ("forecastHourly" in response) {
                var hours = response.forecastHourly.hours;
                for (var i = 0; forecastHours.length < MAX_HOURS && i < hours.length; i += hourInterval) {
                    var hour = hours[i];
                    var forecastHour = parseWKForecastHour(hour, i);
                    forecastHours.push(forecastHour);
                }
            }

            if ("forecastNextHour" in response) {
                debugLog("Next hour summaries:");
                response.forecastNextHour.summary.forEach(function (summary, index) {
                    debugLog("Summary " + index + ": condition=" + summary.condition);
                });
                debugLog("Minutes:");
                response.forecastNextHour.minutes.forEach(function (minute) {
                    debugLog("  " + minute.startTime + ": chance=" + minute.precipitationChance + ", intensity=" + minute.precipitationIntensity);
                });
            }

            var nextHour = "clear";

            if ("forecastNextHour" in response) {
                // Check each summary until we find one that isn't "clear"
                var summariesList = response.forecastNextHour.summary;
                for (var s = 0; s < summariesList.length; s++) {
                    if (summariesList[s].condition !== "clear") {
                        nextHour = summariesList[s].condition;
                        break;
                    }
                }
            }

            // Use weatherkit.processPrecipitationMinutes for precipitation logic
            if ("forecastNextHour" in response) {
                var processed = weatherkit.processPrecipitationMinutes(
                    nextHour,
                    response.forecastNextHour.minutes
                );
                if (processed) {
                    precipitation = msgproc.packPrecipitation(
                        processed.precipitationType,
                        processed.precipitationIntensities
                    );
                } else {
                    // No precipitation detected, send empty data
                    precipitation = msgproc.packPrecipitation(0, filledArray(24, 0));
                }
            } else {
                // No forecastNextHour data available, send empty precipitation data
                precipitation = msgproc.packPrecipitation(0, filledArray(24, 0));
            }

            // Precipitation pin: use first summary that isn't clear
            if ("forecastNextHour" in response) {
                var summaries = response.forecastNextHour.summary;
                if (summaries && summaries.length > 0) {
                    var firstPrecip = null;
                    for (var k = 0; k < summaries.length; k++) {
                        var candidate = summaries[k];
                        if (candidate.condition && candidate.condition !== "clear") {
                            firstPrecip = candidate;
                            break;
                        }
                    }
                    if (firstPrecip) {
                        var startTime = new Date(firstPrecip.startTime);
                        var endTime = firstPrecip.endTime
                            ? new Date(firstPrecip.endTime)
                            : new Date(response.forecastNextHour.forecastEnd);
                        var averageIntensity = firstPrecip.precipitationIntensity || 0;
                        var precipType = firstPrecip.condition.charAt(0).toUpperCase() + firstPrecip.condition.slice(1);
                        var pin = events.createNextHourPrecipitationPin(startTime, endTime, precipType, averageIntensity);
                        if (pin) {
                            timeline.insertUserPin(pin, function (responseText) {
                                debugLog("Precipitation pin pushed: " + responseText);
                            });
                        }
                    }
                }
            }

            debugLog("Forecast hours assembled");
            //debugLog("Forecast hours to send: " + JSON.stringify(forecastHours));

            if (forecastHours.length > 0) {
                // Cache the weather data before sending
                msgproc.saveWeatherCache(forecastHours);

                // Cache precipitation data (always available now)
                msgproc.savePrecipitationCache(precipitation);

                // Send all weather data at once
                sendAllWeatherData();
            } else {
                sendResponseData(1);
            }

        } else {
            debugLog("Error requesting weather data: " + xhr.status);
        }
    };

    xhr.onerror = function () {
        debugLog("Network error occurred while requesting weather data");
    };

    xhr.open(method, url);
    xhr.setRequestHeader("Authorization", "Bearer " + API_KEY);
    xhr.send();
}

function requestWeatherAvailability(location) {
    debugLog("Requesting weather availability");
    //var location = "35.602/-77.345";
    var url = availabilityURL + location;
    var xhr = new XMLHttpRequest();

    xhr.onload = function () {
        if (xhr.status === 200) {
            debugLog("Weather availability response: " + xhr.responseText);
            // If availability check is successful, request the actual weather data
            //requestWeatherData(location);
            var dataSets = "";

            if (xhr.responseText.indexOf("forecastHourly") !== -1) {
                dataSets += "forecastHourly";

                if (xhr.responseText.indexOf("forecastNextHour") !== -1) {
                    dataSets += ",forecastNextHour";
                }

            }else{
                debugLog("No data sets available");
                sendResponseData(1);
                return;
            }

            requestWeatherData(location, dataSets);
        } else {
            debugLog("Error requesting weather availability: " + xhr.status);
            sendResponseData(1);
            return;
        }
    };

    xhr.onerror = function () {
        debugLog("Network error occurred while requesting weather availability");
    };

    xhr.open(method, url);
    xhr.setRequestHeader("Authorization", "Bearer " + API_KEY);
    xhr.send();
}

function parseWKForecastHour(hour) {
    var conditionKey = weatherkit.getConditionKey(hour.conditionCode);
    if (hour.daylight === false) {
        if (conditionKey === 0) conditionKey = 11;
        else if (conditionKey === 2) conditionKey = 12;
    }

    var forecastHour = msgproc.packHourData(
        getTimeStringHour(hour.forecastStart),
        Math.round(celsiusToFahrenheit(hour.temperature)),
        Math.round(celsiusToFahrenheit(hour.temperatureApparent)),
        Math.round(hour.windSpeed),
        "windGust" in hour ? Math.round(hour.windGust) : -1, //not required in response
        Math.round(hour.visibility/1000),
        Math.round(hour.pressure),
        "windDirection" in hour ? hour.windDirection : -1, //not required in response
        -1, //no air quality in response
        hour.uvIndex,
        conditionKey,
        weatherkit.getExperientialIcon(hour)
    );

    return forecastHour;
}

// Export the debugLog function so it can be used by other modules
module.exports.debugLog = debugLog;


function formatTimeString(utcString) {
    // Create date object from UTC string
    var date = new Date(utcString);

    // Get hour in local time
    var hour = date.getHours();

    // Convert to 12-hour format
    var period = 'AM';
    if (hour >= 12) {
        period = 'PM';
        if (hour > 12) {
            hour -= 12;
        }
    }
    if (hour === 0) {
        hour = 12;
    }

    return hour + period;
}

function getTimeStringHour(utcString) {
    var date = new Date(utcString);
    var hour = date.getHours();

    return hour;
}
