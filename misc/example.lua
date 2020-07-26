io.write("hello\n")

--[[
bitmap = Bitmap.load("misc/tile.gif")

W,H = bitmap:size()

io.write("Object: " .. bitmap:__tostring() .. ": " .. W .. "x" .. H .. "\n")
]]

bitmap = Bitmap.create(200, 100)
tile = Bitmap.load("misc/tile.gif")

--bitmap:clip(30,15, 60, 50)

bitmap:setColor(1, 0, 0)
bitmap:print(10, 10, "Hello World")

bitmap:resample(300, 150, 'blin')

bitmap:blit(tile, 30, 30)

-- font = Bitmap.Font.loadSFont('3rd-party/SaikyoBlack.png')
font = Bitmap.Font.loadRaster('fonts/font.gif')

bitmap:setFont(font)
bitmap:print(10, 60, "Hello Font")

bitmap:setFont()
bitmap:print(10, 70, "Hello Font")

bitmap:save('out-lua.bmp')
