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

for i = 1,#slots do
	local slot = slots[i];
	print("namespace slotdefs");
	print("{");
	ssyms = {};
	for j=1,#(slot.slots) do
		local xslot = slot.slots[j];
		local ssym = makesymbol();
		local ssym2 = makesymbol();
		table.insert(ssyms, ssym2);
		local s;
		s = "\tcore_romimage_info "..ssym2.."{{";
		s = s .. "\"" .. xslot.iname .. "\", ";
		s = s .. "\"" .. xslot.hname .. "\", ";
		s = s .. xslot.mandatory .. ", ";
		s = s .. xslot.mode .. ", ";
		s = s .. xslot.header;
		s = s .. "}};";
		print(s);
	end
	print("}");
	local s;
	s = "std::vector<core_romimage_info*> "..slot.symbol.."{";
	for j = 1,#ssyms do
		s = s .. "&slotdefs::" .. ssyms[j] .. ",";
	end
	s = stripcomma(s);
	s = s .. "};";
	print(s);
end
