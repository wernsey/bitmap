
-- Create a bitmap
bitmap = Bitmap.create(200, 100)

--
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

bitmap:line(200, 100, 300, 150)
bitmap:rect(200, 10, 250, 60)
bitmap:fillrect(200, 60, 250, 110)
bitmap:dithrect(250, 10, 300, 60)
bitmap:setColor("#FF0055")
bitmap:circle(225, 135, 25)
bitmap:setColor("#5500FF")
bitmap:fillcircle(270, 135, 20)

bitmap:setColor("#FF00FF")
bitmap:ellipse(210, 20, 290, 50)
bitmap:setColor("#00FFFF")
bitmap:fillellipse(110, 20, 200, 50)
bitmap:setColor("#0000FF")
bitmap:roundrect(210, 70, 240, 100, 4)
bitmap:setColor("#FF5500")
bitmap:fillroundrect(210, 120, 240, 150, 5)

bitmap:bezier3(10, 140, 10, 130, 30, 125)

message = "Hello Font"

-- Load an SFont:
sfont = Bitmap.Font.loadSFont('3rd-party/SaikyoBlack.png')
bitmap:setFont(sfont)
bitmap:print(20, 80, message)

-- Or load a raster font
rfont = Bitmap.Font.loadRaster('fonts/font.gif')
bitmap:setFont(rfont)
bitmap:print(30, 90, message)

W,H = bitmap:textSize(message)
io.write("Text size: " .. W .. "x" .. H .. "\n")

bitmap:setFont() -- reset the font
bitmap:print(10, 70, message)

-- Write the bitmap to a file
bitmap:save('out-lua.gif')

-- The `canvas` variable is a Bitmap that got loaded into the Lua VM from the C program
canvas:setFont(sfont)
canvas:print(10, 10, "Hello Canvas")

-- The old `canvas` is still retained by the C program, so replacing it on the Lua side won't work.
canvas = Bitmap.create(120, 80)
bitmap:setColor("#FF5500")
canvas:print(10, 10, "Hello Canvas")

io.write("Done.\n")