// Debug configuration
var debug = false;

function debugLog(message) {
    if (debug) {
        console.log(message);
    }
}

const conditionCodes = ["Clear", "Cloudy", "Dust", "Fog", "Haze", "MostlyClear", "MostlyCloudy",
    "PartlyCloudy", "ScatteredThunderstorms", "Smoke", "Breezy", "Windy",
    "Drizzle", "HeavyRain", "Rain", "Showers", "Flurries", "HeavySnow",
    "MixedRainAndSleet", "MixedRainAndSnow", "MixedRainfall", "MixedSnowAndSleet",
    "ScatteredShowers", "ScatteredSnowShowers", "Sleet", "Snow", "SnowShowers",
    "Blizzard", "BlowingSnow", "FreezingDrizzle", "FreezingRain", "Frigid",
    "Hail", "Hot", "Hurricane", "IsolatedThunderstorms", "SevereThunderstorm",
    "Thunderstorm", "Tornado", "TropicalStorm"]

const conditionCodeMapping = {
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
}

const precipitationTypes = ["clear", "precipitation", "rain", "snow", "sleet", "hail", "mixed"]

/**
 * Converts a condition code to a numerical key (0-15)
 * @param {string} conditionCode - The weather condition code to convert
 * @returns {number} A numerical key between 0-15 representing the condition category
 */
function getConditionKey(conditionCode) {
    debugLog("Getting condition key for: " + conditionCode);
    // Create an array of category keys for consistent indexing
    const categories = Object.keys(conditionCodeMapping);
    
    // Find the category that contains the condition code
    for (let i = 0; i < categories.length; i++) {
        if (conditionCodeMapping[categories[i]].includes(conditionCode)) {
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
// 1: mask / bad air quality (not in weatherkit though 😢)
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
    
    // 5: hat + scarf / really cold (<= -5°C)
    if (forecastHour.temperatureApparent <= -5) {
        return 5;
    }
    
    // 4: hat / cold (<= 4°C)
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
        let now = new Date();
        let firstPrecipMinute = new Date(precipitationMinutes[0].startTime);

        // If current time is before first minute, prepend empty entries
        if (now < firstPrecipMinute) {
            let minutesToAdd = [];
            let currentMinute = new Date(now);
            currentMinute.setMinutes(now.getMinutes());

            while (currentMinute < firstPrecipMinute) {
                minutesToAdd.push({
                    startTime: currentMinute.toISOString(),
                    precipitationChance: 0,
                    precipitationIntensity: 0
                });
                currentMinute.setMinutes(currentMinute.getMinutes() + 1);
            }

            precipitationMinutes = [...minutesToAdd, ...precipitationMinutes];
        }

        // Filter precipitation minutes to only include those at 5-minute intervals
        precipitationMinutes = precipitationMinutes.filter(minute => {
            let minuteTime = new Date(minute.startTime);
            return minuteTime.getMinutes() % 5 === 0;
        });

        // Ensure array is exactly 24 entries (2 hours of 5-minute intervals)
        while (precipitationMinutes.length < 24) {
            let lastEntry = precipitationMinutes[precipitationMinutes.length - 1];
            let lastTime = new Date(lastEntry.startTime);
            lastTime.setMinutes(lastTime.getMinutes() + 5);
            precipitationMinutes.push({
                startTime: lastTime.toISOString(),
                precipitationChance: 0,
                precipitationIntensity: 0
            });
        }

        // Find the maximum precipitation intensity
        let maxIntensity = Math.max(...precipitationMinutes.map(minute => minute.precipitationIntensity));

        // Scale all intensities to 0-3 range and extract just the intensity values
        // Use ceiling so only exactly 0 intensity gets value 0
        let scaledIntensities = precipitationMinutes.map(minute => {
            if (maxIntensity > 0 && minute.precipitationIntensity > 0) {
                return Math.min(3, Math.ceil((minute.precipitationIntensity / maxIntensity) * 3));
            }
            return 0;
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
    conditionCodes,
    conditionCodeMapping,
    getConditionKey,
    getPrecipitationIndex,
    getExperientialIcon,
    processPrecipitationMinutes
}

