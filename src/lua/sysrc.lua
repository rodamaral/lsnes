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
memory2=memory2();
callback=callback();
render_queue_function=function(rq)
	local _rq = rq;
	return function()
		_rq:run();
	end;
end;
string.byteU=_lsnes_string_byteU;
string.charU=_lsnes_string_charU;
