module.exports = [
    {
      "type": "heading",
      "defaultValue": "Dribble"
    },
    {
      "type": "section",
      "items": [
        {
          "type": "heading",
          "defaultValue": "Units"
        },
        {
          "type": "select",
          "messageKey": "CFG_TEMPERATURE_UNITS",
          "defaultValue": "F",
          "label": "Temperature",
          "options": [
            {
              "label": "°F",
              "value": "F"
            },
            {
              "label": "°C", 
              "value": "C"
            }
          ]
        },
        {
          "type": "select",
          "messageKey": "CFG_VELOCITY_UNITS",
          "defaultValue": "mph",
          "label": "Wind speed",
          "options": [
            {
              "label": "mph",
              "value": "mph"
            },
            {
              "label": "km/h",
              "value": "kph"
            },
            {
              "label": "m/s",
              "value": "m/s"
            }
          ]
        },
        {
          "type": "select", 
          "messageKey": "CFG_DISTANCE_UNITS",
          "defaultValue": "mi",
          "label": "Visibility",
          "options": [
            {
              "label": "miles",
              "value": "mi"
            },
            {
              "label": "kilometers",
              "value": "km"
            }
          ]
        },
        {
          "type": "select",
          "messageKey": "CFG_PRESSURE_UNITS",
          "defaultValue": "mb",
          "label": "Pressure",
          "options": [
            {
              "label": "inHg",
              "value": "in"
            },
            {
              "label": "mb",
              "value": "mb"
            }
          ]
        },
        {
          "type": "select",
          "messageKey": "CFG_PRECIPITATION_UNITS",
          "defaultValue": "in",
          "label": "Precipitation",
          "options": [
            {
              "label": "in",
              "value": "in"
            },
            {
              "label": "mm",
              "value": "mm"
            }
          ]
        }
      ]
    },
    {
      "type": "section",
      "items": [
        {
            "type": "heading",
            "defaultValue": "Functionality"
          },
                  {
            "type": "select",
            "messageKey": "CFG_REFRESH_INTERVAL",
            "defaultValue": 30,
            "label": "Maximum data age",
            "options": [
              {
                "label": "Maximum data age",
                "value": [
                  { 
                    "label": "20 minutes",
                    "value": 20 
                  },
                  { 
                    "label": "30 minutes",
                    "value": 30 
                  },
                  { 
                    "label": "1 hour",
                    "value": 60 
                  }
                ]
              }
            ]
          },
          {
            "type": "toggle",
            "messageKey": "CFG_SELF_REFRESH",
            "label": "Enable self-refresh",
            "description": "Allows the app to schedule itself to open and refresh weather data, and close right after. This is probably necessary for precipitation to show up in the timeline. (not yet activated)",
            "defaultValue": true
          },
          {
            "type": "select",
            "messageKey": "CFG_DISPLAY_INTERVAL",
            "defaultValue": 2,
            "label": "Forecast display interval",
            "options": [
              {
                "label": "Forecast display interval",
                "value": [
                  { 
                    "label": "1 hour",
                    "value": 1 
                  },
                  { 
                    "label": "2 hours",
                    "value": 2 
                  }
                ]
              }
            ]
          }
      ]
    },
    {
      "type": "section",
      "items": [
        {
          "type": "heading",
          "defaultValue": "Appearance"
        },
        {
          "type": "toggle",
          "messageKey": "CFG_ANIMATE",
          "label": "Enable animations",
          "defaultValue": true
        },
        {
          "type": "select",
          "messageKey": "CFG_WIND_VANE_DIRECTION",
          "defaultValue": 0,
          "label": "Wind vane icon points to...",
          "options": [
            {
              "label": "Wind heading",
              "value": 0
            },
            {
              "label": "Wind origin",
              "value": 1
            }
          ]
        },
      ]
    },
    {
      "type": "section",
      "items": [
        {
          "type": "heading",
          "defaultValue": "Attribution"
        },
        {
          "type": "text",
          "defaultValue": "View the Apple WeatherKit data sources <a href='https://developer.apple.com/weatherkit/data-source-attribution/'>here</a>."
        }
      ]
    },
    {
      "type": "submit",
      "defaultValue": "Save Settings"
    }
  ];