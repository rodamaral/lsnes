nextsym = 0;

stripcomma = function(y)
	if #y > 0 and string.sub(y, #y - 1) == "," then
		return string.sub(y, 0, #y - 1);
	end
	return y;
end

makesymbol = function()
	nextsym = nextsym + 1;
	return "X"..nextsym;
end

if not arg[1] then
	error("Expected input file");
end
dofile(arg[1]);

for i = 1,#regions do
	local region = regions[i];
	print("extern core_type "..region.core..";")
end
emitted = {};
for i = 1,#regions do
	local region = regions[i];
	local srname, xregion;
	for srname,xregion in pairs(region.regions) do
		if not emitted[xregion.symbol] then
			local s;
			s = "core_region "..xregion.symbol.."{{";
			s = s .. "\"" .. xregion.iname .. "\", ";
			s = s .. "\"" .. xregion.hname .. "\", ";
			s = s .. xregion.priority .. ", ";
			s = s .. xregion.id .. ", ";
			s = s .. (xregion.temporary and "true" or "false") .. ", ";
			s = s .. "{" .. xregion.fps[2] .. ", " .. xregion.fps[1] .. "}, ";
			s = s .. "{";
			for k = 1,#(xregion.compatible) do
				s = s .. xregion.compatible[k] .. ",";
			end
			s = stripcomma(s);
			s = s .. "}}};";
			print(s);
		end
		emitted[xregion.symbol] = true;
	end
end
for i = 1,#regions do
	local region = regions[i];
	local s;
	s = "std::vector<core_region*> "..region.symbol.."{";
	for srname,xregion in pairs(region.regions) do
		s = s .. "&" .. xregion.symbol .. ", ";
	end
	s = stripcomma(s);
	s = s .. "};";
	print(s);
end
print("namespace regiondefs");
print("{");
for i = 1,#regions do
	local region = regions[i];
	for srname,xregion in pairs(region.regions) do
		if type(srname) == "string" then
			local csym = makesymbol();
			print("\tcore_sysregion "..csym.."(\""..srname.."\", "..region.core..", "..xregion.symbol..
				");");
		end
	end
end
print("}");
