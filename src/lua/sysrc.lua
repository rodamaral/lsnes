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
