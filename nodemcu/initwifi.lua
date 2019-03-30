station_cfg={}
station_cfg.ssid="Valhalla"
station_cfg.pwd="cloudypanda668"

wifi.setmode(wifi.STATION)
wifi.sta.config(station_cfg)
wifi.sta.connect()

tmr.alarm(1, 1000, 1, function()
    if wifi.sta.getip() == nil then
        print("IP unavailable, Waiting...")
    else
        tmr.stop(1)
        print("ESP8266 mode is: " .. wifi.getmode())
        print("The module MAC address is: " .. wifi.ap.getmac())
        print("Config done, IP is " .. wifi.sta.getip())
    end
end)


