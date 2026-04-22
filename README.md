# Dribble
A precipitation & hyperlocal weather app for Pebble. This app was created within the Cursor editor.

### Cool Stuff
- The KiMaybe library is a custom library for Pebble that allows for smooth-looking, slice-based animations for PDC images (similar to what the system can do). Find it in the `app/src/c/gfx/kimaybe/` directory.
- There is a lot of weather-adjacent iconography, available in the `app/resources/` directory in both SVG and PDC formats. Feel free to use it!

### Making it Work
1. Install the [Pebble SDK](https://developer.rebble.io/sdk/)
2. Generate obfuscated key material with `stuff/obfuscator.js`:
   - Open `stuff/obfuscator.js` in a Node REPL or script and call:
     - `generateObfuscatedApiKey("<YOUR_WEATHERKIT_API_KEY>")`
   - Copy `result.cConfig` into `app/src/c/utils/keykey.h`
   - Copy `result.apiKeysJs` into `app/src/pkjs/api_keys.js` so it exports `API_KEY_OBF` as a `Uint8Array`.
3. Confirm `app/src/pkjs/api_keys.js` exists and does **not** contain a plaintext `API_KEY`.
4. Navigate to the `app/` directory and run `npm install` to install dependencies.
5. Run `pebble build` to build the app.
6. Run `pebble install --phone <your phone's IP address>` to install to your watch.
7. Bob's your uncle!

### Donate
- If you think this app deserves money, you can donate at [fund.loganhead.net](https://fund.loganhead.net)