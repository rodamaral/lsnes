fnname = arg[1];
src = tonumber(arg[2]);
dest = tonumber(arg[3]);
a = tonumber(arg[4]);
bits8 = (arg[5] == "8");

coeffscale = 16384;

coeffs_for = function(xpos, srcsize, dstsize, alpha)
	local ret = {};
	local center = math.floor(xpos * srcsize / dstsize);
	for i = -alpha + 1, alpha do
		local point = center + i;
		local x = point + 0.5 - xpos * srcsize / dstsize - 0.5 * srcsize / dstsize;
		if x < -1e-10 or x > 1e-10 then
			local xpi = x * math.pi;
			if point >= 0 and point < srcsize then
				ret[point] = a * math.sin(xpi) * math.sin(xpi / a) / (xpi * xpi);
			end
		else
			if point >= 0 and point < srcsize then
				ret[point] = 1;
			end
		end
	end
	local sum = 0;
	local k, v;
	for k, v in pairs(ret) do
		sum = sum + v;
	end
	for k, v in pairs(ret) do
		ret[k] = math.floor(coeffscale * ret[k] / sum + 0.5);
	end
	return ret;
end

emit_pixel = function(xpos, srcsize, dstsize, alpha)
	local t = coeffs_for(xpos, srcsize, dstsize, alpha);
	io.stdout:write("tmp[" .. xpos .. "]=(");
	for k,v in pairs(t) do
		io.stdout:write(v .. "*src[" .. k .. "]+");
	end
	io.stdout:write("0)/" .. coeffscale .. ";\n");
end

emit_fn_prologue = function(name, dstsize, b8f)
	if b8f then
		io.stdout:write("void " .. name .. "(unsigned char* dst, unsigned char* src)\n");
	else
		io.stdout:write("void " .. name .. "(unsigned short* dst, unsigned short* src)\n");
	end
	io.stdout:write("{\n");
	io.stdout:write("int tmp[" .. dstsize .. "];\n");
end

emit_fn_epilogue = function(dstsize, b8f)
	io.stdout:write("for(int i = 0; i < " .. dstsize .. "; i++)\n");
	io.stdout:write("if(tmp[i] < 0) dst[i] = 0;\n");
	if b8f then
		io.stdout:write("else if(tmp[i] > 255) dst[i] = 255;\n");
	else
		io.stdout:write("else if(tmp[i] > 65535) dst[i] = 65535;\n");
	end
	io.stdout:write("else dst[i] = tmp[i];\n");
	io.stdout:write("}\n");
end

emit_fn = function(name, srcsize, dstsize, alpha, b8f)
	emit_fn_prologue(name, dstsize, b8f);
	for i = 0, dstsize - 1 do
		emit_pixel(i, srcsize, dstsize, alpha)
	end
	emit_fn_epilogue(dstsize, b8f);
end

emit_fn(fnname, src, dest, a, bits8);
