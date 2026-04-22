// Debug configuration
var debug = false;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}

var conditionCodes = ["Clear", "Cloudy", "Dust", "Fog", "Haze", "MostlyClear", "MostlyCloudy",
    "PartlyCloudy", "ScatteredThunderstorms", "Smoke", "Breezy", "Windy",
    "Drizzle", "HeavyRain", "Rain", "Showers", "Flurries", "HeavySnow",
    "MixedRainAndSleet", "MixedRainAndSnow", "MixedRainfall", "MixedSnowAndSleet",
    "ScatteredShowers", "ScatteredSnowShowers", "Sleet", "Snow", "SnowShowers",
    "Blizzard", "BlowingSnow", "FreezingDrizzle", "FreezingRain", "Frigid",
    "Hail", "Hot", "Hurricane", "IsolatedThunderstorms", "SevereThunderstorm",
    "Thunderstorm", "Tornado", "TropicalStorm"];

var conditionCodeMapping = {
    // Clear conditions
    "Clear": ["Clear", "MostlyClear", "Hot", "Frigid"],

    // Cloudy conditions
    "Cloudy": ["Cloudy", "MostlyCloudy"],

    // Partly cloudy conditions
    "Partly Cloudy": ["PartlyCloudy"],

    // Rain conditions
    "Rain": ["Drizzle", "Rain", "Showers", "ScatteredShowers", "HeavyRain", "TropicalStorm"],

    // Snow conditions
    "Snow": ["Snow", "SnowShowers", "ScatteredSnowShowers", "HeavySnow", "Flurries", "Blizzard"],

    // Thunder conditions
    "Thunder": ["Thunderstorm", "IsolatedThunderstorms", "ScatteredThunderstorms", "SevereThunderstorm"],

    // Mixed precipitation
    "Mixed": ["MixedRainAndSleet", "MixedRainAndSnow", "MixedRainfall", "MixedSnowAndSleet",
              "FreezingDrizzle", "FreezingRain", "Sleet"],

    // Severe conditions
    //Moved ["Blizzard", "Hurricane", "Tornado", "TropicalStorm"] to other things
    //because they probably won't be encountered very often
    "Severe": [],

    // Wind conditions
    "Windy": ["Breezy", "Windy", "BlowingSnow", "Hurricane", "Tornado"],

    // Fog conditions
    "Foggy": ["Fog", "Haze", "Dust", "Smoke"],

    // Hail
    "Hail": ["Hail"]
};

var precipitationTypes = ["clear", "precipitation", "rain", "snow", "sleet", "hail", "mixed"];

/**
 * Converts a condition code to a numerical key (0-15)
 * @param {string} conditionCode - The weather condition code to convert
 * @returns {number} A numerical key between 0-15 representing the condition category
 */
function getConditionKey(conditionCode) {
    debugLog("Getting condition key for: " + conditionCode);
    // Create an array of category keys for consistent indexing
    var categories = Object.keys(conditionCodeMapping);

    // Find the category that contains the condition code
    for (var i = 0; i < categories.length; i++) {
        if (conditionCodeMapping[categories[i]].indexOf(conditionCode) !== -1) {
            return i;
        }
    }

    // Default to 0 (Clear) if no match is found
    return 0;
}

function getPrecipitationIndex(precipitationType) {
    return precipitationTypes.indexOf(precipitationType);
}

// Experiential icons:
// 0: none
// 1: mask / bad air quality (not in weatherkit though)
// 2: baseball cap / medium UV index
// 3: sunglasses / high UV index
// 4: hat / cold
// 5: hat + scarf / really cold
// 6: umbrella / wet precipitation
// 7: fog / low visibility
function getExperientialIcon(forecastHour) {

    // 6: umbrella / wet precipitation
    if (getConditionKey(forecastHour.conditionCode) === 3 || getConditionKey(forecastHour.conditionCode) === 6) {
        return 6;
    }

    // 5: hat + scarf / really cold (<= -5 C)
    if (forecastHour.temperatureApparent <= -5) {
        return 5;
    }

    // 4: hat / cold (<= 4 C)
    if (forecastHour.temperatureApparent <= 4) {
        return 4;
    }

    // 7: fog / low visibility
    if (getConditionKey(forecastHour.conditionCode) === 9 || forecastHour.visibility < 2000) {
        return 7;
    }

    // 3: sunglasses / high UV index (>= 7)
    if (forecastHour.uvIndex >= 7) {
        return 3;
    }

    // 2: baseball cap / medium UV index (>= 3)
    if (forecastHour.uvIndex >= 3) {
        return 2;
    }

    // 0: none
    return 0;
}


/**
 * Processes precipitation minutes and returns processed data without packing.
 * @param {string} nextHour - The precipitation type/condition for the next hour (e.g., 'rain', 'clear', etc.)
 * @param {Array} precipitationMinutes - Array of minute objects with startTime, precipitationChance, precipitationIntensity
 * @returns {Object|null} Processed precipitation data object or null if not enough data
 */
function processPrecipitationMinutes(nextHour, precipitationMinutes) {
    if (nextHour !== "clear" && precipitationMinutes && precipitationMinutes.length > 10) {
        // Get current time and first precipitation minute time
        var now = new Date();
        var firstPrecipMinute = new Date(precipitationMinutes[0].startTime);

        // If current time is before first minute, prepend empty entries
        if (now < firstPrecipMinute) {
            var minutesToAdd = [];
            var currentMinute = new Date(now);
            currentMinute.setMinutes(now.getMinutes());

            while (currentMinute < firstPrecipMinute) {
                minutesToAdd.push({
                    startTime: currentMinute.toISOString(),
                    precipitationChance: 0,
                    precipitationIntensity: 0
                });
                currentMinute.setMinutes(currentMinute.getMinutes() + 1);
            }

            precipitationMinutes = minutesToAdd.concat(precipitationMinutes);
        }

        // Filter precipitation minutes to only include those at 5-minute intervals
        precipitationMinutes = precipitationMinutes.filter(function (minute) {
            var minuteTime = new Date(minute.startTime);
            return minuteTime.getMinutes() % 5 === 0;
        });

        // Ensure array is exactly 24 entries (2 hours of 5-minute intervals)
        while (precipitationMinutes.length < 24) {
            var lastEntry = precipitationMinutes[precipitationMinutes.length - 1];
            var lastTime = new Date(lastEntry.startTime);
            lastTime.setMinutes(lastTime.getMinutes() + 5);
            precipitationMinutes.push({
                startTime: lastTime.toISOString(),
                precipitationChance: 0,
                precipitationIntensity: 0
            });
        }

        // Map intensities to 0-3 using absolute mm/hr thresholds (consistent with events.js)
        var isSnowLike = (nextHour === "snow" || nextHour === "sleet");
        var scaledIntensities = precipitationMinutes.map(function (minute) {
            var mmhr = minute.precipitationIntensity;
            if (mmhr < 0.1) return 0;
            if (isSnowLike) {
                if (mmhr > 2.5) return 3;
                if (mmhr >= 1.0) return 2;
                return 1;
            } else {
                if (mmhr > 7.6) return 3;
                if (mmhr >= 2.5) return 2;
                return 1;
            }
        });

        return {
            precipitationType: getPrecipitationIndex(nextHour),
            precipitationIntensities: scaledIntensities
        };
    }
    return null;
}

// Export both the original codes and the mapping
module.exports = {
    conditionCodes: conditionCodes,
    conditionCodeMapping: conditionCodeMapping,
    getConditionKey: getConditionKey,
    getPrecipitationIndex: getPrecipitationIndex,
    getExperientialIcon: getExperientialIcon,
    processPrecipitationMinutes: processPrecipitationMinutes
};
