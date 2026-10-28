// Debug configuration
var debug = false;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}

/**
 * Packs weather data into a 10-byte message according to the format:
 * uint8: relative hour (hours from current time)
 * int8: temperature degrees (f)
 * int8: temperature (feels like) degrees (f)
 * uint8: wind speed (kph)
 * uint8: wind gust speed (kph)
 * uint8: visibility (km)
 * int8: pressure difference from 1000mb (-128 mb to 127 mb)
 * uint4: wind direction (0-15) in 22.5 degree increments
 * uint4: air quality index (1-5, 1=good, 5=very poor)
 * uint4: UV index (0-15)
 * uint4: data flags
 * uint4: condition icon code
 * uint4: experiential icon code
 */
function packHourData(
    relativeHour,
    temperature,
    feelsLike,
    windSpeed,
    windGust,
    visibility,
    pressure,
    windDirection,
    airQualityIndex,
    uvIndex,
    condition,
    experientialIcon
) {
    var buffer = new ArrayBuffer(10);
    var view = new DataView(buffer);

    // Relative hour (uint8)
    view.setUint8(0, relativeHour);

    // Temperature (int8)
    view.setInt8(1, Math.round(temperature));

    // Feels like temperature (int8)
    view.setInt8(2, Math.round(feelsLike));

    // Wind speed (uint8)
    view.setUint8(3, Math.round(windSpeed));

    // Wind gust speed (uint8)
    view.setUint8(4, Math.round(windGust));

    // Visibility (uint8)
    view.setUint8(5, Math.round(visibility));

    // Pack pressure, wind direction, air quality index, uv index, and data flags into bytes 6-8
    // Byte 6: int8 pressure difference from 1000mb (-128 to 127)
    // Byte 7: 4 bits wind direction (0-15, 22.5 deg steps), 4 bits air quality index (1-5, 1=good, 5=very poor)
    // Byte 8: 4 bits uv index (0-15), 4 bits data flags (0000 -> empty, windgust, winddir, airquality)
    var pressureDiff = Math.max(-128, Math.min(127, Math.round(pressure) - 1000));
    view.setInt8(6, pressureDiff);

    // Calculate data flags
    var hasWindGust = windGust !== -1;
    var hasWindDir = windDirection !== -1;
    var hasAirQuality = airQualityIndex !== -1 && airQualityIndex >= 1 && airQualityIndex <= 5;
    var dataFlags = (hasWindGust ? 0x4 : 0) | (hasWindDir ? 0x2 : 0) | (hasAirQuality ? 0x1 : 0);

    // Clamp and pack wind direction and air quality index
    // Center wind direction bins: shift by half a bin (11.25 deg) before dividing
    var windDir4 = hasWindDir ? Math.floor(((windDirection + 11.25) % 360) / 22.5) : 0;
    var aqi4 = hasAirQuality ? Math.max(1, Math.min(5, Math.round(airQualityIndex))) : 0;
    view.setUint8(7, (windDir4 << 4) | (aqi4 & 0x0F));

    // Clamp and pack uv index and data flags
    var uv4 = Math.max(0, Math.min(15, Math.round(uvIndex)));
    view.setUint8(8, (uv4 << 4) | (dataFlags & 0x0F));

    // Condition and experiential icons (uint4 each)
    view.setUint8(9, (condition << 4) | (experientialIcon & 0x0F));

    //debugLog("buffer ints: " + bufferToInts(buffer));
    return bufferToInts(buffer);
}


/*
    Special details can be provided to the current hour, such as precipitation

    uint8: precipitation type (0 for none)
    uint2 * [24]: precipitation intensity (on a scale of 0-3) in 5 minute intervals, for up to 2 hours

*/
function packPrecipitation(
    precipitationType,
    precipitationMinutes
) {
    var buffer = new ArrayBuffer(7);
    var view = new DataView(buffer);

    // First byte: precipitation type (uint8)
    view.setUint8(0, precipitationType);

    // Next 6 bytes: 24 precipitation intensity values (2 bits each)
    // Each byte can hold 4 intensity values
    for (var i = 0; i < 6; i++) {
        var byte = 0;
        for (var j = 0; j < 4; j++) {
            var intensityIndex = i * 4 + j;
            if (intensityIndex < precipitationMinutes.length) {
                // Shift each 2-bit value into position
                byte |= (precipitationMinutes[intensityIndex] & 0x03) << (j * 2);
            }
        }
        view.setUint8(i + 1, byte);
    }

    return bufferToInts(buffer);
}

function bufferToInts(buffer) {
    var view = new Uint8Array(buffer);
    var ints = [];
    for (var i = 0; i < view.length; i++) {
        ints.push(view[i]);
    }
    return ints;
}

/**
 * Packs all 12 hour packages into a single 120-byte array for bulk transmission.
 * Each hour package is 10 bytes, resulting in 12 * 10 = 120 bytes total.
 * @param {Array} hourPackages - Array of 12 packed hour data arrays (each 10 bytes)
 * @returns {Array} - Single 120-byte array containing all hour data in order
 */
function packAllHourData(hourPackages) {
    var result = [];
    for (var i = 0; i < 12; i++) {
        if (i < hourPackages.length) {
            // Add existing hour package data
            for (var j = 0; j < hourPackages[i].length; j++) {
                result.push(hourPackages[i][j]);
            }
            // Pad to 10 bytes if needed
            for (var k = hourPackages[i].length; k < 10; k++) {
                result.push(0);
            }
        } else {
            // Pad with zeros for missing hours
            for (var m = 0; m < 10; m++) {
                result.push(0);
            }
        }
    }
    return result;
}

module.exports = {
    packHourData: packHourData,
    packAllHourData: packAllHourData,
    packPrecipitation: packPrecipitation
};
