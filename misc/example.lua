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

bitmap:putpixel(99,49)
bitmap:putpixel(101,49)
bitmap:putpixel(100,50)
bitmap:putpixel(99,51)
bitmap:putpixel(101,51)

bitmap:resample(300, 150, 'blin')

bitmap:blit(tile, 30, 30)

bitmap:setColor("#FFFF00")
io.write(bitmap:getColor())

bitmap:line(200, 100, 300, 150)
bitmap:rect(200, 10, 250, 60)
bitmap:fillrect(200, 60, 250, 110)
bitmap:dithrect(250, 10, 300, 60)
bitmap:circle(225, 135, 25)
bitmap:fillcircle(270, 135, 20)

bitmap:setColor("#FF0000")
bitmap:ellipse(210, 20, 290, 50)
bitmap:roundrect(210, 70, 240, 100, 4)
bitmap:fillroundrect(210, 120, 240, 150, 5)

-- font = Bitmap.Font.loadSFont('3rd-party/SaikyoBlack.png')
font = Bitmap.Font.loadRaster('fonts/font.gif')

bitmap:setFont(font)
bitmap:print(10, 60, "Hello Font")

bitmap:setFont()
bitmap:print(10, 70, "Hello Font")

bitmap:save('out-lua.bmp')
