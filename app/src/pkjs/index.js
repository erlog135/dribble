var keys = require('message_keys');
var msgproc = require('./msgproc');
var leyzux = require('./leyzux');
var openmeteo = require('./openmeteo');
var timeline = require('./timeline');

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

var language = "";

var forecastHours = [];
var precipitation = null;

var MAX_HOURS = 12;
var hourInterval = 2; // Default to 2 hours
var refreshInterval = 30; // Default to 30 minutes
var weatherProvider = "leyzux"; // "leyzux" or "openmeteo"

// Debug configuration
var debug = false;
var checkCache = true;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}

/**
 * Loads settings from localStorage saved by Clay
 */
function loadSettings() {
    if (localStorage.getItem('clay-settings')) {
        try {
            var settings = JSON.parse(localStorage.getItem('clay-settings'));
            hourInterval = parseInt(settings.CFG_DISPLAY_INTERVAL, 10) || 2;
            refreshInterval = parseInt(settings.CFG_REFRESH_INTERVAL, 10) || 30;
            weatherProvider = settings.CFG_WEATHER_PROVIDER || "leyzux";
            debugLog("Settings loaded: provider=" + weatherProvider + ", interval=" + hourInterval + ", refresh=" + refreshInterval);
        } catch (e) {
            debugLog("Error loading clay settings: " + e.message);
        }
    }
}

Pebble.addEventListener("ready",
    function (e) {
        debugLog("PebbleKit JS Ready - Starting automatic weather fetch");
        language = Pebble.getActiveWatchInfo().language;

        loadSettings();

        // Send JSReady signal to C app
        debugLog("Sending JSReady signal to C app");
        Pebble.sendAppMessage({ "JS_READY": 1 }, function () {
            debugLog("JSReady signal sent successfully");
            if (checkCache) {
                checkCacheOrFetchWeather();
            } else {
                getLocation();
            }
        }, function (e) {
            debugLog("JSReady signal failed: " + JSON.stringify(e));
            if (checkCache) {
                checkCacheOrFetchWeather();
            } else {
                getLocation();
            }
        });
    }
);

// When Clay configuration is closed/saved, refresh settings and weather
Pebble.addEventListener("webviewclosed", function (e) {
    if (!e || !e.response) { return; }
    debugLog("Clay settings updated, reloading settings and refreshing weather");
    loadSettings();
    msgproc.clearWeatherCache();
    getLocation();
});

// code 0 for success
// 1: hourly forecast failed
// 2: location error
function sendResponseData(responseCode) {
    Pebble.sendAppMessage({ "RESPONSE_DATA": responseCode }, function () {
        debugLog("Response data sent: " + responseCode);
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

    var cachedData = false;//msgproc.getCachedWeatherData(refreshInterval);

    if (cachedData) {
        debugLog("Using cached weather data");
        forecastHours = cachedData.hourPackages;
        precipitation = cachedData.precipitationData;

        if (forecastHours && forecastHours.length > 0) {
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
            function (position) {
                debugLog("Location obtained successfully");
                var latitude = position.coords.latitude;
                var longitude = position.coords.longitude;
                debugLog("Location: " + latitude + "/" + longitude);

                fetchWeather(latitude, longitude);
            },
            function (error) {
                debugLog("Error getting location: " + error.message);
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
        sendResponseData(2);
    }
}

/**
 * Dispatches weather request to the active provider with automatic fallback.
 */
function fetchWeather(latitude, longitude) {
    forecastHours = [];
    precipitation = null;

    if (weatherProvider === "leyzux") {
        debugLog("Requesting weather from Leyzux proxy (interval=" + hourInterval + ")...");
        leyzux.fetchWeather(latitude, longitude, { hourInterval: hourInterval }, function (err, result) {
            if (err) {
                debugLog("Leyzux Weather failed: " + err.message + ". Falling back to Open-Meteo.");
                fetchOpenMeteo(latitude, longitude);
                return;
            }

            handleWeatherSuccess(result);
        });
    } else {
        debugLog("Requesting weather directly from Open-Meteo...");
        fetchOpenMeteo(latitude, longitude);
    }
}

/**
 * Fetches weather from Open-Meteo.
 */
function fetchOpenMeteo(latitude, longitude) {
    openmeteo.fetchWeather(latitude, longitude, {
        hourInterval: hourInterval,
        maxHours: MAX_HOURS
    }, function (err, result) {
        if (err) {
            debugLog("Open-Meteo failed: " + err.message);
            sendResponseData(1);
            return;
        }

        handleWeatherSuccess(result);
    });
}

/**
 * Handles successful weather payload from either provider.
 */
function handleWeatherSuccess(result) {
    forecastHours = result.hourPackages;
    precipitation = result.precipitation;

    if (forecastHours && forecastHours.length > 0) {
        // Cache weather data
        msgproc.saveWeatherCache(forecastHours);
        msgproc.savePrecipitationCache(precipitation);

        // Send all weather data to watch
        sendAllWeatherData();

        // Push timeline pin if one was generated
        if (result.pin) {
            timeline.insertUserPin(result.pin, function (responseText) {
                debugLog("Precipitation pin pushed: " + responseText);
            });
        }
    } else {
        debugLog("No forecast hours received");
        sendResponseData(1);
    }
}

function sendAllWeatherData() {
    debugLog('Starting weather data transmission');

    if (forecastHours.length > 0) {
        debugLog('Packing ' + forecastHours.length + ' forecast hours into single 120-byte message');

        // Pack all hours into a single 120-byte array
        var allHourData = msgproc.packAllHourData(forecastHours);

        // Send all hour data in a single message
        Pebble.sendAppMessage({ "HOUR_DATA": allHourData }, function () {
            debugLog('All hourly data sent successfully! Sending precipitation...');
            if (precipitation != null) {
                debugLog('Sending precipitation data');
                sendPrecipitation();
            } else {
                debugLog('No precipitation data available, sending empty precipitation data');
                var emptyPrecipitation = msgproc.packPrecipitation(0, filledArray(24, 0));
                sendPrecipitationFromData(emptyPrecipitation);
            }
        }, function (e) {
            debugLog('Hour data transmission failed: ' + JSON.stringify(e));
            sendResponseData(1);
        });
    } else {
        debugLog('No forecast hours to send, sending error response');
        sendResponseData(1);
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

module.exports.debugLog = debugLog;
