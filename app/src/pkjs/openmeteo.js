var msgproc = require('./msgproc');
var events = require('./events');

var precipitationTypeNames = [
    "Clear",
    "Precipitation",
    "Rain",
    "Snow",
    "Sleet",
    "Hail",
    "Mixed"
];

function filledArray(length, value) {
    var arr = [];
    for (var i = 0; i < length; i++) {
        arr.push(value);
    }
    return arr;
}

/**
 * Maps WMO Weather Interpretation Codes to Dribble condition indices (0-12)
 * @param {number} code - WMO code
 * @param {boolean} isDay - true if daytime
 * @returns {number} Condition key (0-12)
 */
function mapWmoToCondition(code, isDay) {
    switch (code) {
        case 0: // Clear sky
        case 1: // Mainly clear
            return isDay ? 0 : 11;
        case 2: // Partly cloudy
            return isDay ? 2 : 12;
        case 3: // Overcast
            return 1;
        case 45: // Fog
        case 48: // Depositing rime fog
            return 9;
        case 51: // Light drizzle
        case 53: // Moderate drizzle
        case 55: // Dense drizzle
        case 61: // Slight rain
        case 63: // Moderate rain
        case 65: // Heavy rain
        case 80: // Slight rain showers
        case 81: // Moderate rain showers
        case 82: // Violent rain showers
            return 3;
        case 56: // Light freezing drizzle
        case 57: // Dense freezing drizzle
        case 66: // Light freezing rain
        case 67: // Heavy freezing rain
            return 6;
        case 71: // Slight snow fall
        case 73: // Moderate snow fall
        case 75: // Heavy snow fall
        case 77: // Snow grains
        case 85: // Slight snow showers
        case 86: // Heavy snow showers
            return 4;
        case 95: // Thunderstorm
        case 97: // Heavy thunderstorm
            return 5;
        case 96: // Thunderstorm with slight hail
        case 99: // Thunderstorm with heavy hail
            return 10;
        default:
            return isDay ? 0 : 11;
    }
}

/**
 * Calculates experiential icon (0-7)
 * 0: none
 * 1: mask / bad air quality
 * 2: baseball cap / medium UV index (>= 3)
 * 3: sunglasses / high UV index (>= 7)
 * 4: hat / cold (<= 39 F / 4 C)
 * 5: hat + scarf / really cold (<= 23 F / -5 C)
 * 6: umbrella / wet precipitation (Rain or Mixed)
 * 7: fog / low visibility (Fog or visibility < 2 km)
 * @param {Object} hourData
 * @returns {number} Experiential icon index (0-7)
 */
function getExperientialIcon(hourData) {
    // 6: umbrella / wet precipitation
    if (hourData.condition === 3 || hourData.condition === 6) {
        return 6;
    }
    // 5: hat + scarf / really cold (<= 23 F / -5 C)
    if (hourData.tempApparentF <= 23) {
        return 5;
    }
    // 4: hat / cold (<= 39 F / 4 C)
    if (hourData.tempApparentF <= 39) {
        return 4;
    }
    // 7: fog / low visibility
    if (hourData.condition === 9 || hourData.visibilityKm < 2) {
        return 7;
    }
    // 3: sunglasses / high UV index (>= 7)
    if (hourData.uvIndex >= 7) {
        return 3;
    }
    // 2: baseball cap / medium UV index (>= 3)
    if (hourData.uvIndex >= 3) {
        return 2;
    }
    return 0;
}

/**
 * Processes Open-Meteo minutely_15 precipitation into 24 5-minute slots (0-3)
 * and determines dominant precipitation type.
 * @param {Object} minutely15 - response.minutely_15
 * @returns {Object} { type: number, intensities: Array<number>, hasPrecip: boolean, firstIndex: number, avgIntensity: number }
 */
function processOpenMeteoPrecipitation(minutely15) {
    if (!minutely15 || !minutely15.precipitation || minutely15.precipitation.length === 0) {
        return {
            type: 0,
            intensities: filledArray(24, 0),
            hasPrecip: false,
            firstIndex: -1,
            avgIntensity: 0
        };
    }

    var precipAmounts = minutely15.precipitation;
    var weatherCodes = minutely15.weather_code || [];

    var hasPrecip = false;
    var firstIndex = -1;
    var maxPrecip = 0;
    var dominantCode = 0;
    var sumPrecipNextHour = 0;
    var countNextHour = 0;

    for (var i = 0; i < precipAmounts.length && i < 8; i++) {
        var val = precipAmounts[i] || 0;
        if (i < 4) {
            sumPrecipNextHour += val;
            countNextHour++;
        }
        if (val > 0.05) {
            if (!hasPrecip) {
                firstIndex = i;
            }
            hasPrecip = true;
            if (val > maxPrecip) {
                maxPrecip = val;
                dominantCode = weatherCodes[i] || 0;
            }
        }
    }

    if (!hasPrecip) {
        return {
            type: 0,
            intensities: filledArray(24, 0),
            hasPrecip: false,
            firstIndex: -1,
            avgIntensity: 0
        };
    }

    // Determine precipitation type from dominant WMO code
    // Types: 0: clear, 1: precipitation, 2: rain, 3: snow, 4: sleet, 5: hail, 6: mixed
    var precipType = 2; // Default to Rain
    if ([71, 73, 75, 77, 85, 86].indexOf(dominantCode) !== -1) {
        precipType = 3; // Snow
    } else if ([56, 57, 66, 67].indexOf(dominantCode) !== -1) {
        precipType = 6; // Mixed
    } else if ([96, 99].indexOf(dominantCode) !== -1) {
        precipType = 5; // Hail
    }

    var isSnowLike = (precipType === 3 || precipType === 4);

    // Map 8 15-minute intervals to 24 5-minute slots (each 15m block expands to 3 slots)
    var intensities = [];
    for (var j = 0; j < 8; j++) {
        var mm = j < precipAmounts.length ? (precipAmounts[j] || 0) : 0;
        // Convert 15-min amount to mm/hr rate: mm * 4
        var mmhr = mm * 4;
        var level = 0;
        if (mmhr >= 0.1) {
            if (isSnowLike) {
                if (mmhr > 2.5) level = 3;
                else if (mmhr >= 1.0) level = 2;
                else level = 1;
            } else {
                if (mmhr > 7.6) level = 3;
                else if (mmhr >= 2.5) level = 2;
                else level = 1;
            }
        }
        intensities.push(level);
        intensities.push(level);
        intensities.push(level);
    }

    while (intensities.length < 24) {
        intensities.push(0);
    }

    // Average intensity for the first hour in mm/hr
    var avgIntensity = countNextHour > 0 ? (sumPrecipNextHour * 4 / countNextHour) : 0;

    return {
        type: precipType,
        intensities: intensities,
        hasPrecip: true,
        firstIndex: firstIndex,
        avgIntensity: avgIntensity
    };
}

/**
 * Fetches and encodes weather data from Open-Meteo
 * @param {number|string} latitude
 * @param {number|string} longitude
 * @param {Object} options - { hourInterval: number, maxHours: number }
 * @param {function} callback - Function(err, result)
 */
function fetchWeather(latitude, longitude, options, callback) {
    var hourInterval = (options && options.hourInterval) || 2;
    var maxHours = (options && options.maxHours) || 12;

    var url = 'https://api.open-meteo.com/v1/forecast?' +
        'latitude=' + latitude +
        '&longitude=' + longitude +
        '&hourly=temperature_2m,apparent_temperature,precipitation,weather_code,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,visibility,is_day' +
        '&minutely_15=precipitation,weather_code' +
        '&temperature_unit=fahrenheit' +
        '&wind_speed_unit=kmh' +
        '&precipitation_unit=mm' +
        '&timeformat=iso8601' +
        '&forecast_days=2' +
        '&forecast_minutely_15=8' +
        '&timezone=auto';

    console.log('Requesting Open-Meteo from: ' + url);

    var xhr = new XMLHttpRequest();
    xhr.timeout = 10000;

    xhr.onload = function () {
        if (xhr.status !== 200) {
            callback(new Error('Open-Meteo returned HTTP ' + xhr.status));
            return;
        }

        var response;
        try {
            response = JSON.parse(xhr.responseText);
        } catch (e) {
            callback(new Error('Failed to parse Open-Meteo JSON response: ' + e.message));
            return;
        }

        if (!response || !response.hourly || !response.hourly.time || response.hourly.time.length === 0) {
            callback(new Error('Invalid response structure from Open-Meteo'));
            return;
        }

        var hourly = response.hourly;
        var now = Date.now();
        var startIdx = 0;

        // Find current hour index
        for (var k = 0; k < hourly.time.length; k++) {
            var t = new Date(hourly.time[k]).getTime();
            if (t <= now && (t + 3600000) > now) {
                startIdx = k;
                break;
            }
            if (t > now) {
                startIdx = k > 0 ? k - 1 : 0;
                break;
            }
        }

        var hourPackages = [];
        for (var i = startIdx; hourPackages.length < maxHours && i < hourly.time.length; i += hourInterval) {
            var relativeHour = i - startIdx;

            var temp = Math.round(hourly.temperature_2m[i]);
            var feelsLike = Math.round(hourly.apparent_temperature[i]);
            var windSpeed = Math.round(hourly.wind_speed_10m[i]);

            var windGust = (hourly.wind_gusts_10m && hourly.wind_gusts_10m[i] !== null && !isNaN(hourly.wind_gusts_10m[i]))
                ? Math.round(hourly.wind_gusts_10m[i])
                : -1;

            var visibility = (hourly.visibility && hourly.visibility[i] !== null && !isNaN(hourly.visibility[i]))
                ? Math.round(hourly.visibility[i] / 1000)
                : 10;

            var pressure = (hourly.surface_pressure && hourly.surface_pressure[i] !== null && !isNaN(hourly.surface_pressure[i]))
                ? Math.round(hourly.surface_pressure[i])
                : 1013;

            var windDir = (hourly.wind_direction_10m && hourly.wind_direction_10m[i] !== null && !isNaN(hourly.wind_direction_10m[i]))
                ? Math.round(hourly.wind_direction_10m[i])
                : -1;

            var uvIndex = (hourly.uv_index && hourly.uv_index[i] !== null && !isNaN(hourly.uv_index[i]))
                ? Math.round(hourly.uv_index[i])
                : 0;

            var isDay = (hourly.is_day && hourly.is_day[i] !== undefined)
                ? (hourly.is_day[i] === 1)
                : true;

            var wmoCode = hourly.weather_code ? hourly.weather_code[i] : 0;
            var conditionKey = mapWmoToCondition(wmoCode, isDay);

            var experientialIcon = getExperientialIcon({
                condition: conditionKey,
                tempApparentF: feelsLike,
                visibilityKm: visibility,
                uvIndex: uvIndex
            });

            var packedHour = msgproc.packHourData(
                relativeHour,
                temp,
                feelsLike,
                windSpeed,
                windGust,
                visibility,
                pressure,
                windDir,
                -1, // Air quality index not available
                uvIndex,
                conditionKey,
                experientialIcon
            );

            hourPackages.push(packedHour);
        }

        if (hourPackages.length === 0) {
            callback(new Error('No forecast hours could be assembled from Open-Meteo'));
            return;
        }

        var allHourData = msgproc.packAllHourData(hourPackages);

        // Process minutely_15 precipitation
        var precipInfo = processOpenMeteoPrecipitation(response.minutely_15);
        var precipitationData = msgproc.packPrecipitation(precipInfo.type, precipInfo.intensities);

        // Build precipitation timeline pin if precipitation is detected in the next hour
        var pin = null;
        if (precipInfo.hasPrecip && precipInfo.firstIndex >= 0 && precipInfo.firstIndex < 4) {
            var pinStartTime = new Date(Date.now() + precipInfo.firstIndex * 15 * 60000);
            var pinEndTime = new Date(Date.now() + 60 * 60000);
            var precipTypeStr = (precipInfo.type >= 0 && precipInfo.type < precipitationTypeNames.length)
                ? precipitationTypeNames[precipInfo.type]
                : "Rain";

            pin = events.createNextHourPrecipitationPin(pinStartTime, pinEndTime, precipTypeStr, precipInfo.avgIntensity);
        }

        callback(null, {
            hourPackages: hourPackages,
            allHourData: allHourData,
            precipitation: precipitationData,
            pin: pin
        });
    };

    xhr.onerror = function () {
        callback(new Error('Network error requesting Open-Meteo'));
    };

    xhr.ontimeout = function () {
        callback(new Error('Timeout requesting Open-Meteo'));
    };

    xhr.open('GET', url);
    xhr.send();
}

module.exports = {
    fetchWeather: fetchWeather,
    mapWmoToCondition: mapWmoToCondition,
    getExperientialIcon: getExperientialIcon,
    processOpenMeteoPrecipitation: processOpenMeteoPrecipitation
};
