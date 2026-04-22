/**
 * No-dependency helper to obfuscate an API key for your Pebble flow.
 *
 * Output:
 * - apiKeyObf: byte array for app/src/pkjs/api_keys.js
 * - cConfig: C macros to paste into keykey.h (or msgobf config section)
 *
 * LCG generation matches your C logic:
 *   state = state * MULT + INC (uint32 wrap)
 *   keyByte = (state >> 24) & 0xFF
 * Cipher:
 *   obf[i] = apiKeyByte[i] ^ keystream[i % keystreamLen]
 */
function generateObfuscatedApiKey(apiKey, options) {
    options = options || {};
  
    if (typeof apiKey !== "string" || apiKey.length === 0) {
      throw new Error("apiKey must be a non-empty string");
    }
  
    // Defaults
    var keystreamLen = options.keystreamLen || 16;
    if (keystreamLen <= 0 || keystreamLen > 255) {
      throw new Error("keystreamLen must be between 1 and 255");
    }
  
    // Random uint32 helper (Math.random based; fine for obfuscation use)
    function randomUint32() {
      // Build 32-bit from two 16-bit chunks
      var hi = (Math.random() * 0x10000) | 0;
      var lo = (Math.random() * 0x10000) | 0;
      return (((hi << 16) >>> 0) | lo) >>> 0;
    }
  
    // Pick random seed; keep Numerical Recipes params by default for C parity
    var seed = (options.seed !== undefined ? options.seed >>> 0 : randomUint32());
    var mult = (options.mult !== undefined ? options.mult >>> 0 : 1664525 >>> 0);
    var inc  = (options.inc  !== undefined ? options.inc  >>> 0 : 1013904223 >>> 0);
  
    // ASCII-safe bytes (works for normal API/JWT keys)
    var plainBytes = [];
    for (var i = 0; i < apiKey.length; i++) {
      var code = apiKey.charCodeAt(i);
      if (code > 255) {
        throw new Error("Non-ASCII character at position " + i + ". Use UTF-8 encoding if needed.");
      }
      plainBytes.push(code);
    }
  
    // Build keystream
    var keystream = [];
    var state = seed >>> 0;
    for (var k = 0; k < keystreamLen; k++) {
      state = (((state * mult) >>> 0) + inc) >>> 0;
      keystream.push((state >>> 24) & 0xFF);
    }
  
    // XOR with tiled keystream
    var apiKeyObf = [];
    for (var j = 0; j < plainBytes.length; j++) {
      apiKeyObf.push((plainBytes[j] ^ keystream[j % keystream.length]) & 0xFF);
    }
  
    function toHexU32(n) {
      return "0x" + (n >>> 0).toString(16).toUpperCase().padStart(8, "0") + "u";
    }
  
    return {
      apiKeyObf: apiKeyObf,
      keystreamLen: keystreamLen,
      lcg: { seed: seed >>> 0, mult: mult >>> 0, inc: inc >>> 0 },
  
      // Paste into C config header
      cConfig:
        "#define MSGOBF_SEED " + toHexU32(seed) + "\n" +
        "#define MSGOBF_MULT " + (mult >>> 0) + "u\n" +
        "#define MSGOBF_INC " + (inc >>> 0) + "u\n" +
        "#define MSGOBF_KEYSTREAM_LEN " + keystreamLen,
  
      // Paste into app/src/pkjs/api_keys.js
      apiKeysJs:
        "module.exports.API_KEY_OBF = new Uint8Array([\n  " +
        apiKeyObf.map(function (b) { return "0x" + b.toString(16).toUpperCase().padStart(2, "0"); }).join(", ") +
        "\n]);"
    };
  }