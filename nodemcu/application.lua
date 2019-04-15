-- stuff
dofile("sgp30.lua")


-- url with node mcu access token
URL = "http://192.168.1.15:8080/api/v1/md5XHApKw4UMDAmOfB1R/telemetry"


-- Log
print("DOING application.lua")


-- Config
local pin = 4            --> GPIO2
local value = gpio.LOW


-- Function toggles LED state
function toggleLED ()
    if value == gpio.LOW then
        value = gpio.HIGH
    else
        value = gpio.LOW
    end
    gpio.write(pin, value)
end


-- Periodically check in that we are alive
function sendPing ()
    http.post(URL, 'Content-Type: application/json\r\n', '{"ping":1}',
        function(code, data)
            if (code < 0) then
                print("ping failed with code "..code)
            end
        end
    )
end


-- Initialise the pin
gpio.mode(pin, gpio.OUTPUT)
gpio.write(pin, value)


-- Create an interval
tmr.alarm(0, 1000, tmr.ALARM_AUTO, toggleLED)
tmr.alarm(1, 60000, tmr.ALARM_AUTO, sendPing)


-- Log
print("DONE application.lua")
