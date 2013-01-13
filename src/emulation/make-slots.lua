nextsym = 0;

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
		s = "\tstruct core_romimage_info_params "..ssym.." = {";
		s = s .. "\"" .. xslot.iname .. "\", ";
		s = s .. "\"" .. xslot.hname .. "\", ";
		s = s .. xslot.mandatory .. ", ";
		s = s .. xslot.mode .. ", ";
		s = s .. xslot.header;
		s = s .. "};";
		print(s);
		print("\tcore_romimage_info "..ssym2.."("..ssym..");");
	end
	print("}");
	local s;
	s = "core_romimage_info* "..slot.symbol.."[] = {";
	for j = 1,#ssyms do
		s = s .. "&slotdefs::" .. ssyms[j] .. ",";
	end
	s = s .. "NULL};";
	print(s);
end
