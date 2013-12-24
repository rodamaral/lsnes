#!/usr/bin/env lua

for line in io.stdin:lines() do
	c,name = string.match(line, "#(%x+) ([^ \t]+)");
	if name then
		print("{\""..name.."\", [](int64_t& v) { v = 0x"..c.."; }, false},");
	end
end
