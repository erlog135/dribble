const { getConditionKey } = require('./weatherkit');

const WEATHER_CACHE_KEY = 'weather_cache_data';
const PRECIPITATION_CACHE_KEY = 'precipitation_cache_data';

// Debug configuration
var debug = false;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}

/**
 * Packs weather data into a 10-byte message according to the format:
 * uint8: hour
 * int8: temperature degrees (f)
 * int8: temperature (feels like) degrees (f)
 * uint8: wind speed (kph)
 * uint8: wind gust speed (kph)
 * uint8: visibility (km)
 * int8: pressure difference from 1000mb (-128 mb to 127 mb)
 * uint4: wind direction (0-15) in 22.5 degree increments
 * uint4: air quality index (0-15) 0-500 in steps of 50
 * uint4: UV index (0-15)
 * uint4: data flags
 * uint4: condition icon code
 * uint4: experiential icon code
 */
function packHourData(
    hour,
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
    const buffer = new ArrayBuffer(10);
    const view = new DataView(buffer);

    // Hour (uint8)
    view.setUint8(0, hour);

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
    // Byte 7: 4 bits wind direction (0-15, 22.5 deg steps), 4 bits air quality index (0-15, 0-500 in steps of 50)
    // Byte 8: 4 bits uv index (0-15), 4 bits data flags (0000 -> empty, windgust, winddir, airquality)
    const pressureDiff = Math.max(-128, Math.min(127, Math.round(pressure) - 1000));
    view.setInt8(6, pressureDiff);

    // Calculate data flags
    const hasWindGust = windGust !== -1;
    const hasWindDir = windDirection !== -1;
    const hasAirQuality = airQualityIndex !== -1;
    const dataFlags = (hasWindGust ? 0x4 : 0) | (hasWindDir ? 0x2 : 0) | (hasAirQuality ? 0x1 : 0);

    // Clamp and pack wind direction and air quality index
    // Center wind direction bins: shift by half a bin (11.25°) before dividing
    const windDir4 = hasWindDir ? Math.floor(((windDirection + 11.25) % 360) / 22.5) : 0;
    const aqi4 = hasAirQuality ? Math.max(0, Math.min(15, Math.round(airQualityIndex / 50))) : 0;
    view.setUint8(7, (windDir4 << 4) | (aqi4 & 0x0F));

    // Clamp and pack uv index and data flags
    const uv4 = Math.max(0, Math.min(15, Math.round(uvIndex)));
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
    const buffer = new ArrayBuffer(7);
    const view = new DataView(buffer);

    // First byte: precipitation type (uint8)
    view.setUint8(0, precipitationType);

    // Next 6 bytes: 24 precipitation intensity values (2 bits each)
    // Each byte can hold 4 intensity values
    for (let i = 0; i < 6; i++) {
        let byte = 0;
        for (let j = 0; j < 4; j++) {
            const intensityIndex = i * 4 + j;
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
    const view = new Uint8Array(buffer);
    const ints = [];
    for (let i = 0; i < view.length; i++) {
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
    const result = [];
    for (let i = 0; i < 12; i++) {
        if (i < hourPackages.length) {
            // Add existing hour package data
            for (let j = 0; j < hourPackages[i].length; j++) {
                result.push(hourPackages[i][j]);
            }
            // Pad to 10 bytes if needed
            for (let j = hourPackages[i].length; j < 10; j++) {
                result.push(0);
            }
        } else {
            // Pad with zeros for missing hours
            for (let j = 0; j < 10; j++) {
                result.push(0);
            }
        }
    }
    return result;
}

/**
 * Saves weather hour packages to localStorage with timestamp
 * @param {Array} hourPackages - Array of packed weather hour data
 */
function saveWeatherCache(hourPackages) {
    try {
        const cacheData = {
            timestamp: Date.now(),
            hourPackages: hourPackages
        };
        localStorage.setItem(WEATHER_CACHE_KEY, JSON.stringify(cacheData));
        debugLog('Weather data cached successfully');
    } catch (error) {
        debugLog('Failed to save weather cache: ' + error.message);
    }
}

/**
 * Saves precipitation data to localStorage with timestamp
 * @param {Array} precipitationData - Packed precipitation data
 */
function savePrecipitationCache(precipitationData) {
    try {
        const cacheData = {
            timestamp: Date.now(),
            precipitationData: precipitationData
        };
        localStorage.setItem(PRECIPITATION_CACHE_KEY, JSON.stringify(cacheData));
        debugLog('Precipitation data cached successfully');
    } catch (error) {
        debugLog('Failed to save precipitation cache: ' + error.message);
    }
}

/**
 * Retrieves cached weather data if it exists and is within the specified age limit
 * @param {number} maxAgeMinutes - Maximum age of cached data in minutes
 * @returns {Object|null} - Object with hourPackages and precipitationData, or null if no valid cache
 */
function getCachedWeatherData(maxAgeMinutes) {
    try {
        // Get weather cache
        const weatherCacheStr = localStorage.getItem(WEATHER_CACHE_KEY);
        const precipitationCacheStr = localStorage.getItem(PRECIPITATION_CACHE_KEY);
        
        if (!weatherCacheStr) {
            debugLog('No weather cache found');
            return null;
        }
        
        const weatherCache = JSON.parse(weatherCacheStr);
        const now = Date.now();
        const maxAgeMs = maxAgeMinutes * 60 * 1000;
        const weatherAge = now - weatherCache.timestamp;
        
        if (weatherAge > maxAgeMs) {
            debugLog('Weather cache expired (age: ' + Math.round(weatherAge / (60 * 1000)) + ' minutes, max: ' + maxAgeMinutes + ' minutes)');
            return null;
        }
        
        const result = {
            hourPackages: weatherCache.hourPackages,
            precipitationData: null
        };
        
        // Check precipitation cache if it exists
        if (precipitationCacheStr) {
            const precipitationCache = JSON.parse(precipitationCacheStr);
            const precipitationAge = now - precipitationCache.timestamp;
            
            if (precipitationAge <= maxAgeMs) {
                result.precipitationData = precipitationCache.precipitationData;
                debugLog('Using cached precipitation data (age: ' + Math.round(precipitationAge / (60 * 1000)) + ' minutes)');
            } else {
                debugLog('Precipitation cache expired (age: ' + Math.round(precipitationAge / (60 * 1000)) + ' minutes)');
            }
        }
        
        debugLog('Using cached weather data (age: ' + Math.round(weatherAge / (60 * 1000)) + ' minutes)');
        return result;

    } catch (error) {
        debugLog('Failed to retrieve weather cache: ' + error.message);
        return null;
    }
}

/**
 * Clears all weather-related cache data
 */
function clearWeatherCache() {
    try {
        localStorage.removeItem(WEATHER_CACHE_KEY);
        localStorage.removeItem(PRECIPITATION_CACHE_KEY);
        debugLog('Weather cache cleared');
    } catch (error) {
        debugLog('Failed to clear weather cache: ' + error.message);
    }
}

module.exports = {
    packHourData,
    packAllHourData,
    packPrecipitation,
    saveWeatherCache,
    savePrecipitationCache,
    getCachedWeatherData,
    clearWeatherCache
};
