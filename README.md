# ESP32 firmware to display 8x8 icons coming from IconViewer web app

## APIs

All of these should respond to `OPTIONS` correctly and support CORS (from any origin)`.

* `/brightness`
    * `PUT` a `text/plain` brightness value (0-255)
* `/correction`
    * `PUT` a `text/plain` LED type correction enum; 0-5 where 0 is `UncorrectedColor` and 1-4 are [the possible values from FastLED](https://fastled.io/docs/3.1/group___color_enums.html), in order
* `/temperature`
    * `PUT` a `text/plain` color temperature enum; 0-5 where 0 is `UncorrectedTemperature` and 1-19 are [the possible values from FastLED](https://fastled.io/docs/3.1/group___color_enums.html), in order
* `/icon`
    * `POST` an 'application/octet-stream' representing an icon (see protocol definition below)
    * `DELETE` the currently displayed icon

## Icon protocol definition
```    
byte 0: number of frames
bytes 1-4: 32 bit int duration of frame in ms
bytes 5-192: 8x8x3 frame data (row major, 0 in top left, RGB (no alpha channel) color data for one frame)
bytes 193-196: duration of next frame
... etc
```