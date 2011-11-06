s = "";
for i = 128,255 do
	s = s .. string.char(i);
end
io.stdout:write(s);