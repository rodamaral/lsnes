loopwrapper = function(fn, ...)
	local routine = coroutine.create(fn);
	local resume = function(...)
		if coroutine.status(routine) ~= "dead" then
			local x, y;
			x, y = coroutine.resume(routine, ...);
			if not x then
				error(y);
			end
		end
	end
	local yield = function()
		return coroutine.yield(routine);
	end
	resume(yield, ...);
	return resume;
end;
print=print2;
loadfile=loadfile2;
dofile=dofile2;
render_queue_function=function(rq)
	local _rq = rq;
	return function()
		_rq:run();
	end;
end;
string.byteU=_lsnes_string_byteU;
string.charU=_lsnes_string_charU;
string.regex=_lsnes_string_regex;
string.hex=_lsnes_string_hex;
string.lpad=_lsnes_string_lpad;
string.rpad=_lsnes_string_rpad;

local _lookup_class = lookup_class;
local _all_classes = all_classes;
local classes_meta = {
	["__newindex"] = function() error("Classes table is not writable"); end,
	["__index"] = function(a, b) return _lookup_class(b); end
};
classes = {};
setmetatable(classes, classes_meta);

local register_in = function(table, class)
	local methods = {classes[class]._static_methods()};
	local utable;
	if table then
		_G[table] = _G[table] or {};
		utable = _G[table];
	else
		utable = _G;
	end
	local i;
	for i=1,#methods do
		local m = methods[i];
		utable[m] = classes[class][m];
	end
end

-- Classes
memory.address = classes.ADDRESS;
memory.mmap = classes.MMAP_STRUCT;
zip.writer = classes.ZIPWRITER;
gui.tiled_bitmap = classes.TILEMAP;
gui.renderctx = classes.RENDERCTX;
gui.palette = classes.PALETTE;
gui.bitmap = classes.BITMAP;
gui.dbitmap = classes.DBITMAP;
gui.image = classes.IMAGELOADER;
gui.font = classes.CUSTOMFONT;
iconv = classes.ICONV;
filereader = classes.FILEREADER;

-- Some ctors
memory2=classes.VMALIST.new();
callback=classes.CALLBACKS_LIST.new();
memory.mkaddr = classes.ADDRESS.new;
memory.map_structure=classes.MMAP_STRUCT.new;
memory.compare_new=classes.COMPARE_OBJ.new;
zip.create=classes.ZIPWRITER.new;
gui.tilemap=classes.TILEMAP.new;
gui.renderq_new=classes.RENDERCTX.new;
gui.palette_new=classes.PALETTE.new;
gui.font_new = classes.CUSTOMFONT.new;
gui.loadfont = classes.CUSTOMFONT.load;
iconv_new = classes.ICONV.new;
create_ibind = classes.INVERSEBIND.new;
create_command = classes.COMMANDBIND.new;
open_file = classes.FILEREADER.open;

local do_arg_err = function(what, n, name)
	error("Expected "..what.." as argument #"..n.." of "..name);
end

local normal_method = {}

local normal_method = function(class, method, name, parent)
	if type(class) == "string" then
		normal_method[name] = normal_method[name] or {};
		normal_method[name][class] = method;
	else
		normal_method[name] = normal_method[name] or {};
		local k, v;
		for k, v in pairs(class) do
			normal_method[name][v] = method;
		end
	end
	parent[name] = function(o, ...)
		local m = normal_method[name];
		local c = identify_class(o);
		if m[c] then
			return o[m[c]](o, ...);
		else
			local what = "";
			local k, v;
			for k, v in pairs(m) do
				if what ~= "" then
					what = what .. " or " .. k;
				else
					what = k;
				end
			end
			do_arg_err(what, 1, name);
		end
	end
end

gui.renderq_set=function(o, ...)
	if type(o) == "nil" then
		return classes.RENDERCTX.setnull();
	elseif identify_class(o) == "RENDERCTX" then
		return o:set(...);
	else
		do_arg_err("RENDERCTX or nil", 1, "gui.renderq_set");
	end
end

normal_method("RENDERCTX", "run", "renderq_run", gui);
normal_method("RENDERCTX", "synchronous_repaint", "synchronous_repaint", gui);
normal_method("RENDERCTX", "clear", "renderq_clear", gui);

gui.bitmap_new=function(w, h, type, ...)
	if type==true then
		return classes.DBITMAP.new(w,h,...);
	elseif type==false then
		return classes.BITMAP.new(w,h,...);
	else
		do_arg_err("boolean", 3, "gui.bitmap_new");
	end
end

gui.bitmap_draw=function(x, y, o, ...)
	if identify_class(o) == "BITMAP" then
		return o:draw(x, y, ...);
	elseif identify_class(o) == "DBITMAP" then
		return o:draw(x, y, ...);
	else
		do_arg_err("BITMAP or DBITMAP", 3, "gui.bitmap_draw");
	end
end

gui.bitmap_save_png=function(...)
	local x = {...};
	local i = 1;
	local j;
	local obj;
	for j=1,#x do
		if type(x[j]) ~= "string" then
			obj = table.remove(x, j);
			i = j;
			break;
		end
	end
	if identify_class(obj) == "BITMAP" then
		return obj:save_png(unpack(x));
	elseif identify_class(obj) == "DBITMAP" then
		return obj:save_png(unpack(x));
	else
		do_arg_err("BITMAP or DBITMAP", i, "gui.bitmap_save_png");
	end
end

gui.bitmap_load=classes.IMAGELOADER.load;
gui.bitmap_load_str=classes.IMAGELOADER.load_str;
gui.bitmap_load_png=classes.IMAGELOADER.load_png;
gui.bitmap_load_png_str=classes.IMAGELOADER.load_png_str;

normal_method("PALETTE", "set", "palette_set", gui);
normal_method("PALETTE", "hash", "palette_hash", gui);
normal_method("PALETTE", "debug", "palette_debug", gui);
normal_method({"BITMAP", "DBITMAP"}, "pset", "bitmap_pset", gui);
normal_method({"BITMAP", "DBITMAP"}, "pget", "bitmap_pget", gui);
normal_method({"BITMAP", "DBITMAP"}, "size", "bitmap_size", gui);
normal_method({"BITMAP", "DBITMAP"}, "hash", "bitmap_hash", gui);
normal_method({"BITMAP", "DBITMAP"}, "blit", "bitmap_blit", gui);
normal_method({"BITMAP", "DBITMAP"}, "blit_scaled", "bitmap_blit_scaled", gui);
normal_method({"BITMAP", "DBITMAP"}, "blit_porterduff", "bitmap_blit_porterduff", gui);
normal_method({"BITMAP", "DBITMAP"}, "blit_scaled_porterduff", "bitmap_blit_scaled_porterduff", gui);
normal_method("BITMAP", "blit_priority", "bitmap_blit_priority", gui);
normal_method("BITMAP", "blit_scaled_priority", "bitmap_blit_scaled_priority", gui);
normal_method({"DBITMAP", "PALETTE"}, "adjust_transparency", "adjust_transparency", gui);
