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
--Bit aliases
bit.bnot=bit.none;
bit.bor=bit.any;
bit.band=bit.all;
bit.bxor=bit.parity;

local _lookup_class = lookup_class;
memory2=_lookup_class("VMALIST").new();
callback=_lookup_class("CALLBACKS_LIST").new();
memory.map_structure=_lookup_class("MMAP_STRUCT").new;
zip.create=_lookup_class("ZIPWRITER").new;
gui.tilemap=_lookup_class("TILEMAP").new;
gui.renderq_new=_lookup_class("RENDERCTX").new;

local do_arg_err = function(what, n, name)
	error("Expected "..what.." as argument #"..n.." of "..name);
end

gui.renderq_set=function(o, ...)
	if type(o) == "nil" then
		_lookup_class("RENDERCTX").setnull();
	elseif identify_class(o) == "RENDERCTX" then
		o:set(...);
	else
		do_arg_err("RENDERCTX or nil", 1, "gui.renderq_set");
	end
end

gui.renderq_run=function(o, ...)
	if identify_class(o) == "RENDERCTX" then
		o:run(...);
	else
		do_arg_err("RENDERCTX", 1, "gui.renderq_run");
	end
end

gui.synchronous_repaint=function(o, ...)
	if identify_class(o) == "RENDERCTX" then
		o:synchronous_repaint(...);
	else
		do_arg_err("RENDERCTX", 1, "gui.synchronous_repaint");
	end
end

gui.renderq_clear=function(o, ...)
	if identify_class(o) == "RENDERCTX" then
		o:clear(...);
	else
		do_arg_err("RENDERCTX", 1, "gui.renderq_clear");
	end
end
