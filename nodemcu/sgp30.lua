-- Log
print("DOING sgp30.lua")


local dev_addr = 0x58 -- sgp30
local sda = 1 -- gpio5
local scl = 2 -- gpio4



sgp30 = {
    init = function(self, sda, scl)
        self.id = 0
        i2c.setup(self.id, sda, scl, i2c.SLOW)
    end,


    read_reg = function(self, dev_addr, reg_addr)
        i2c.start(self.id)
        i2c.address(self.id, dev_addr ,i2c.TRANSMITTER)
        i2c.write(self.id, reg_addr)
        i2c.stop(self.id)
        i2c.start(self.id)
        i2c.address(self.id, dev_addr,i2c.RECEIVER)
        c = i2c.read(self.id, 2)
        i2c.stop(self.id)
        return c
    end,


    readTemp = function(self)
        h, l = string.byte(self:read_reg(0x1F, 0x05), 1, 2)
        h1=bit.band(h,0x1F)
        --check if Ta > 0C or Ta<0C
        Sgn = bit.band(h,0x10)             
        -- transform - CLEAR Sing BIT if Ta < 0C
        h2 = bit.band(h1,0x0F)
        tp = h2*16+l/16
        --END calculate temperature for Ta > 0
        return tp
    end
}


sgp30:init(sda, scl)

-- Log
print("DONE sgp30.lua")
