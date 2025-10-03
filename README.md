# Dribble
A precipitation & hyperlocal weather app
for Pebble (platforms basalt and beyond)

**Big Disclaimer**
Most of the code in this app was generated using the Cursor editor. For this reason, you may encounter excessively verbose files, unnecessary helper functions or abstractions, and other syntactic superflousness.
In some cases, it will have also provided helpful comments and good code!

**Cool Stuff**
- The KiMaybe library is a custom library for Pebble that allows for smooth-looking, slice-based animations for PDC images (similar to what the system can do). Find it in the `app/src/c/gfx/kimaybe/` directory.
- There is a lot of weather-adjacent iconography, available in the `app/resources/` directory in both SVG and PDC formats. Feel free to use it!

**Making it Work**
1. Install the [Pebble SDK](https://developer.rebble.io/sdk/)
2. add a file `api_keys.js` to the `app/src/pkjs/` directory, with an exported `API_KEY` string that is a valid API key for the Apple WeatherKit REST API.
3. Navigate to the `app/` directory and run `npm install` to install the dependencies.
4. do `pebble build` to build the app.
5. do `pebble install --phone <your phone's IP address>` to install the app to your watch.
6. Bob's your uncle!