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

button="BUTTON";
axis="AXIS";
raxis="RAXIS";
taxis="TAXIS";
shadow="SHADOW";
shadow_axis="SHADOW_AXIS";
null="NULL";
shadow_null="SHADOW_NULL";

if not arg[1] then
	error("Expected input file");
end
dofile(arg[1]);

buttonsymbols = {};

for i = 1,#ports do
	local port = ports[i];
	print("namespace portdefs {");
	local bits = 0;
	local ints = 0;
	local csyms = {};
	local psym = makesymbol();
	local psym2 = makesymbol();
	for j = 1,#(port.controllers) do
		local controller = port.controllers[j];
		local csym = makesymbol();
		local csym2 = makesymbol();
		table.insert(csyms, csym2);
		local bsyms={};
		for k = 1,#(controller.buttons) do
			local xbutton = controller.buttons[k];
			local bsym = makesymbol();
			if xbutton[1] == button then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_BUTTON, U'" .. xbutton[2] ..
					"', \"" .. xbutton[3] .. "\", false, 0, 0, false, \"" ..
					(xbutton[5] or xbutton[2]) .. "\"}";
				bits = bits + 1;
			end
			if xbutton[1] == axis then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_AXIS, U'\\0',\"" .. xbutton[2]
					.. "\", false, ".. xbutton[3] .. ", ".. xbutton[4] .. "," ..
					(xbutton[5] and "true" or "false") .. ", NULL}";
				ints = ints + 1;
			end
			if xbutton[1] == raxis then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_RAXIS, U'\\0',\"" .. xbutton[2]
					.. "\", false, ".. xbutton[3] .. ", ".. xbutton[4] .. "," ..
					(xbutton[5] and "true" or "false") .. ", NULL}";
				ints = ints + 1;
			end
			if xbutton[1] == taxis then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_TAXIS, U'\\0',\"" .. xbutton[2]
					.. "\", false, ".. xbutton[3] .. ", ".. xbutton[4] .. "," ..
					(xbutton[5] and "true" or "false") .. ", NULL}";
				ints = ints + 1;
			end
			if xbutton[1] == shadow then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_BUTTON, U'" .. xbutton[2] ..
						"', \"" .. xbutton[3] .. "\", true, 0, 0, false, NULL}";
				bits = bits + 1;
			end
			if xbutton[1] == shadow_axis then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_AXIS, U'\\0', \"" .. xbutton[2]
					.. "\", true, 0, 0, false, NULL}";
				ints = ints + 1;
			end
			if xbutton[1] == null then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_NULL, U'\\0', NULL, false, 0, 0,"
					.. "false, NULL}";
			end
			if xbutton[1] == shadow_null then
				table.insert(bsyms, bsym);
				buttonsymbols[bsym] = "{port_controller_button::TYPE_NULL, U'\\0', NULL, true, 0, 0,"
					.. "false, NULL}";
			end
		end
		print("\tport_controller "..csym2.." = {\""..controller.class.."\", \""..controller.name.."\", {");
		for k = 1,#bsyms do
			print("\t\t" .. buttonsymbols[bsyms[k]] .. ",");
		end
		print("\t}};");
	end
	local s = "{";
	local f = true;
	for j = 1,#csyms do
		if not f then
			s = s .. ",";
		end
		f = false;
		s = s .."&"..csyms[j];
	end
	s = s .. "}"
	print("\tport_controller_set "..psym2.." = {"..s.."};");
	print("}");
	print("struct _"..port.symbol.." : public port_type");
	print("{");
	print("\t_"..port.symbol.."() : port_type(\""..port.iname.."\", \""..port.hname.."\", "..i..","..
		(math.floor((bits + 7) / 8) + 2 * ints)..")");
	print("\t{");
	print("\t\twrite = [](unsigned char* buffer, unsigned idx, unsigned ctrl, short x) -> void {");
	print("\t\t\tswitch(idx) {");
	local bit_l = 0;
	local int_l = math.floor((bits + 7) / 8);
	for j = 1,#(port.controllers) do
		local controller = port.controllers[j];
		print("\t\t\tcase "..(j-1)..":");
		print("\t\t\t\tswitch(ctrl) {");
		for k = 1,#(controller.buttons) do
			local xbutton = controller.buttons[k];
			local bt = xbutton[1];
			print("\t\t\t\tcase "..(k-1)..":");
			if (bt == button) or (bt == shadow) then
				local bidx = math.floor(bit_l / 8);
				local bmask = math.pow(2, bit_l % 8);
				print("\t\t\t\t\tif(x) buffer["..bidx.."] |= "..bmask.."; else "..
					"buffer["..bidx.."] &= ~"..bmask..";");
				bit_l = bit_l + 1;
			end
			if (bt == axis) or (bt == raxis) or (bt == taxis) or (bt == shadow_axis) then
				print("\t\t\t\t\tbuffer["..int_l.."] = (unsigned short)x;");
				print("\t\t\t\t\tbuffer["..(int_l + 1).."] = ((unsigned short)x >> 8);");
				int_l = int_l + 2;
			end
			print("\t\t\t\t\tbreak;");
		end
		print("\t\t\t\t};");
		print("\t\t\t\tbreak;");
	end
	print("\t\t\t};");
	print("\t\t};");
	print("\t\tread = [](const unsigned char* buffer, unsigned idx, unsigned ctrl) -> short {");
	local bit_l = 0;
	local int_l = math.floor((bits + 7) / 8);
	print("\t\t\tswitch(idx) {");
	for j = 1,#(port.controllers) do
		local controller = port.controllers[j];
		print("\t\t\tcase "..(j-1)..":");
		print("\t\t\t\tswitch(ctrl) {");
		for k = 1,#(controller.buttons) do
			local xbutton = controller.buttons[k];
			local bt = xbutton[1];
			print("\t\t\t\tcase "..(k-1)..":");
			if (bt == button) or (bt == shadow) then
				local bidx = math.floor(bit_l / 8);
				local bmask = math.pow(2, bit_l % 8);
				print("\t\t\t\t\treturn (buffer["..bidx.."] & "..bmask..") ? 1 : 0;");
				bit_l = bit_l + 1;
			end
			if (bt == axis) or (bt == raxis) or (bt == taxis) or (bt == shadow_axis) then
				print("\t\t\t\t\treturn (short)((unsigned short)buffer["..int_l.."] + ("..
					"(unsigned short)buffer["..(int_l+1).."] << 8));");
				int_l = int_l + 2;
			end
			if (bt == null) or (bt == shadow_null) then
				print("\t\t\t\t\treturn 0;");
			end
		end
		print("\t\t\t\t};");
		print("\t\t\t\tbreak;");
	end
	print("\t\t\t};");
	print("\t\t\treturn 0;");
	print("\t\t};");
	print("\t\tserialize = [](const unsigned char* buffer, char* textbuf) -> size_t {");
	local bit_l = 0;
	local int_l = math.floor((bits + 7) / 8);
	print("\t\t\tsize_t ptr = 0;");
	print("\t\t\tshort tmp;");
	for j = 1,#(port.controllers) do
		local controller = port.controllers[j];
		if j > 1 or port.legal[1] ~= 0 then
			print("\t\t\ttextbuf[ptr++] = '|';");
		end
		for k = 1,#(controller.buttons) do
			local xbutton = controller.buttons[k];
			local bt = xbutton[1];
			if (bt == button) or (bt == shadow) then
				local bidx = math.floor(bit_l / 8);
				local bmask = math.pow(2, bit_l % 8);
				print("\t\t\ttextbuf[ptr++] = (buffer["..bidx.."] & "..bmask..") ? '"..
					(xbutton[4] or xbutton[2]).."' : '.';");
				bit_l = bit_l + 1;
			end
		end
		for k = 1,#(controller.buttons) do
			local xbutton = controller.buttons[k];
			local bt = xbutton[1];
			if (bt == axis) or (bt == raxis) or (bt == taxis) or (bt == shadow_axis) then
				print("\t\t\t\ttmp = (short)((unsigned short)buffer["..int_l.."] + ("..
					"(unsigned short)buffer["..(int_l+1).."] << 8));");
				print("\t\t\t\tptr += sprintf(textbuf + ptr, \" %i\", tmp);");
				int_l = int_l + 2;
			end
		end
	end
	print("\t\t\ttextbuf[ptr] = '\\0';");
	print("\t\t\treturn ptr;");
	print("\t\t};");
	print("\t\tdeserialize = [](unsigned char* buffer, const char* textbuf) -> size_t {");
	local bit_l = 0;
	local int_l = math.floor((bits + 7) / 8);
	if #(port.controllers) == 0 then
		print("\t\t\treturn DESERIALIZE_SPECIAL_BLANK;");
	else
		print("\t\t\tmemset(buffer, 0, "..(math.floor((bits + 7) / 8) + 2 * ints)..");");
		print("\t\t\tsize_t ptr = 0;");
		print("\t\t\tshort tmp;");
		for j = 1,#(port.controllers) do
			local controller = port.controllers[j];
			for k = 1,#(controller.buttons) do
				local xbutton = controller.buttons[k];
				local bt = xbutton[1];
				if (bt == button) or (bt == shadow) then
					local bidx = math.floor(bit_l / 8);
					local bmask = math.pow(2, bit_l % 8);
					print("\t\t\tif(read_button_value(textbuf, ptr))");
					print("\t\t\t\tbuffer["..bidx.."] |= "..bmask..";");
					bit_l = bit_l + 1;
				end
			end
			for k = 1,#(controller.buttons) do
				local xbutton = controller.buttons[k];
				local bt = xbutton[1];
				if (bt == axis) or (bt == raxis) or (bt == taxis) or (bt == shadow_axis) then
					print("\t\t\ttmp = read_axis_value(textbuf, ptr);");
					print("\t\t\tbuffer["..int_l.."] = (unsigned short)tmp;");
					print("\t\t\tbuffer["..(int_l + 1).."] = ((unsigned short)tmp >> 8);");
					int_l = int_l + 2;
				end
			end
			print("\t\t\tskip_rest_of_field(textbuf, ptr, "..((j < #(port.controllers)) and 'true' or
				'false')..");");
		end
		print("\t\t\treturn ptr;");
	end
	print("\t\t};");
	print("\t\tlegal = [](unsigned c) -> int {");
	for j = 1,#(port.legal) do
		print("\t\t\tif(c == "..port.legal[j]..") return true;");
	end
	print("\t\t\treturn false;");
	print("\t\t};");
	print("\t\tcontroller_info = &portdefs::"..psym2..";");
	print("\t}");
	print("} "..port.symbol..";");
end
local s = "std::vector<port_type*> _port_types{ ";
for i = 1,#ports do
	s = s .."&"..ports[i].symbol..", ";
end
s = stripcomma(s);
s = s .. "};";
print(s);
