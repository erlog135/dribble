var events = require('./events');

var LEYZUX_BASE_URL = 'https://raincloud.leyzux.com/dribble';

var precipitationTypeNames = [
    "Clear",
    "Precipitation",
    "Rain",
    "Snow",
    "Sleet",
    "Hail",
    "Mixed"
];

/**
 * Portable Base64 string to Uint8 array decoder for PebbleKit JS
 * @param {string} b64 - Base64 encoded string
 * @returns {Array<number>} Array of byte integers (0-255)
 */
function decodeBase64(b64) {
    if (typeof atob === 'function') {
        try {
            var binStr = atob(b64);
            var result = [];
            for (var i = 0; i < binStr.length; i++) {
                result.push(binStr.charCodeAt(i));
            }
            return result;
        } catch (e) {
            // Fall back to pure JS decoder if atob throws
        }
    }

    var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
    var lookup = {};
    for (var c = 0; c < chars.length; c++) {
        lookup[chars.charAt(c)] = c;
    }

    var cleanB64 = b64.replace(/[^A-Za-z0-9+/=]/g, '');
    var bytes = [];
    var len = cleanB64.length;

    for (var j = 0; j < len; j += 4) {
        var b0 = lookup[cleanB64.charAt(j)];
        var b1 = lookup[cleanB64.charAt(j + 1)];
        var b2 = cleanB64.charAt(j + 2) === '=' ? 0 : lookup[cleanB64.charAt(j + 2)];
        var b3 = cleanB64.charAt(j + 3) === '=' ? 0 : lookup[cleanB64.charAt(j + 3)];

        bytes.push((b0 << 2) | (b1 >> 4));
        if (cleanB64.charAt(j + 2) !== '=') {
            bytes.push(((b1 & 15) << 4) | (b2 >> 2));
        }
        if (cleanB64.charAt(j + 3) !== '=') {
            bytes.push(((b2 & 3) << 6) | b3);
        }
    }

    return bytes;
}

/**
 * Fetches and processes weather data from the Leyzux Weather proxy API.
 * 
 * @param {number|string} latitude - Location latitude
 * @param {number|string} longitude - Location longitude
 * @param {Object|number|function} optionsOrCallback - Options object ({ hourInterval: 1|2 }) or callback
 * @param {function} [callback] - Function(err, result)
 */
function fetchWeather(latitude, longitude, optionsOrCallback, callback) {
    var options = {};
    var cb = callback;
    if (typeof optionsOrCallback === 'function') {
        cb = optionsOrCallback;
    } else if (typeof optionsOrCallback === 'object' && optionsOrCallback !== null) {
        options = optionsOrCallback;
    } else if (typeof optionsOrCallback === 'number') {
        options = { hourInterval: optionsOrCallback };
    }

    var interval = options.hourInterval || options.interval || 2;
    var url = LEYZUX_BASE_URL + '/' + latitude + '/' + longitude +
        '?interval=' + encodeURIComponent(interval);

    console.log('Requesting Leyzux Weather from: ' + url);

    var xhr = new XMLHttpRequest();
    xhr.timeout = 10000;

    xhr.onload = function () {
        if (xhr.status !== 200) {
            cb(new Error('Leyzux Weather returned HTTP ' + xhr.status));
            return;
        }

        var response;
        try {
            response = JSON.parse(xhr.responseText);
        } catch (e) {
            cb(new Error('Failed to parse Leyzux Weather JSON response: ' + e.message));
            return;
        }

        if (!response || !response.success || !response.weather || !response.weather.package) {
            cb(new Error('Invalid response structure from Leyzux Weather'));
            return;
        }

        var bytes = decodeBase64(response.weather.package);
        if (bytes.length !== 120 && bytes.length !== 127) {
            cb(new Error('Unexpected byte length from Leyzux Weather: ' + bytes.length + ' bytes (expected 120 or 127)'));
            return;
        }

        // Extract 120 bytes of hourly data
        var allHourData = bytes.slice(0, 120);

        // Slice into 12 10-byte packages for cache compatibility
        var hourPackages = [];
        for (var h = 0; h < 12; h++) {
            hourPackages.push(allHourData.slice(h * 10, (h + 1) * 10));
        }

        // Extract or default 7-byte precipitation data
        var precipitation;
        if (bytes.length === 127) {
            precipitation = bytes.slice(120, 127);
        } else {
            // Empty 7-byte precipitation package (type 0, 24 zero intensities)
            precipitation = [0, 0, 0, 0, 0, 0, 0];
        }

        // Generate precipitation timeline pin if summary is available
        var pin = null;
        if (response.weather.precipitation) {
            var precipSummary = response.weather.precipitation;
            if (typeof precipSummary.starts_in === 'number' && typeof precipSummary.stops_in === 'number') {
                var now = Date.now();
                var startTime = new Date(now + precipSummary.starts_in * 60000);
                var endTime = new Date(now + precipSummary.stops_in * 60000);

                var durationHours = Math.max(0.1, (precipSummary.stops_in - precipSummary.starts_in) / 60);
                var accumMm = precipSummary.accumulation || 0;
                var avgIntensityMmHr = durationHours > 0 ? (accumMm / durationHours) : accumMm;

                var typeIndex = precipitation[0];
                var precipTypeStr = (typeIndex >= 0 && typeIndex < precipitationTypeNames.length)
                    ? precipitationTypeNames[typeIndex]
                    : "Rain";

                pin = events.createNextHourPrecipitationPin(startTime, endTime, precipTypeStr, avgIntensityMmHr, hourPackages, interval);
            }
        }

        cb(null, {
            hourPackages: hourPackages,
            allHourData: allHourData,
            precipitation: precipitation,
            pin: pin
        });
    };

    xhr.onerror = function () {
        cb(new Error('Network error requesting Leyzux Weather'));
    };

    xhr.ontimeout = function () {
        cb(new Error('Timeout requesting Leyzux Weather'));
    };

    xhr.open('GET', url);
    xhr.send();
}

module.exports = {
    fetchWeather: fetchWeather,
    decodeBase64: decodeBase64
};
