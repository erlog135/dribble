# Dribble
A precipitation & hyperlocal weather app for Pebble. This app was created within the Cursor editor.

### Cool Stuff
- The KiMaybe library is a custom library for Pebble that allows for smooth-looking, slice-based animations for PDC images (similar to what the system can do). Find it in the `app/src/c/gfx/kimaybe/` directory.
- There is a lot of weather-adjacent iconography, available in the `app/resources/` directory in both SVG and PDC formats. Feel free to use it!

### Weather Providers
Dribble uses two keyless weather backends (configurable in the app's Clay settings):
- **Leyzux Weather (`leyzux.js`)**: The default proxy API (`raincloud.leyzux.com`), which returns pre-packaged binary weather and hyperlocal precipitation data sourced from Apple WeatherKit and OpenWeather.
- **Open-Meteo (`openmeteo.js`)**: Used either directly when selected in settings or as an automatic fallback if Leyzux Weather is unavailable.

### Making it Work
1. Install the [Pebble SDK](https://developer.repebble.com/sdk/)
2. Navigate to the `app/` directory and run `npm install` to install the dependencies.
3. do `pebble build` to build the app.
4. do `pebble install --phone <your phone's IP address>` to install the app to your watch.
5. Bob's your uncle!

### Donate
- If you think this app deserves money, you can donate at [fund.loganhead.net](https://fund.loganhead.net)
