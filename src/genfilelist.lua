#!/usr/bin/env lua

emit=function(filename)
	local prefix, file, line, err, word;
	if string.match(filename, "%.%./(.*)") then
		return;
	end
	prefix = string.match(filename, "(.*/)[^/]*");
	if not prefix then
		io.stdout:write(" " .. filename);
		return;
	end
	file, err = io.open(filename, "r");
	if not file then
		error(err);
	end
	s = "";
	for line in file:lines() do
		for word in string.gmatch(line, "%S+") do
			s = s .. " " .. prefix .. word;
		end
	end
	io.stdout:write(s);
end

for i=1,#arg do
	emit(arg[i]);
end
print("");
