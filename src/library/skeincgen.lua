XGen = function(permutation, okeys, rc, rounds, name)
local keys = {};
for i=1,okeys,1 do keys[i] = "key["..(i-1).."]"; end
table.insert(keys, "k");
local tweaks={"tweak[0]","tweak[1]","t"};
local subkey = 0;
local current_permute = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

print("static void "..name.."(uint64_t* out, const uint64_t* in, const uint64_t* key, const uint64_t* tweak)");
print("{");
local s="";
for i=0,okeys-1 do s=s.."key["..i.."]^"; end
print("\tuint64_t k="..s.."0x1BD11BDAA9FC1A22ULL;");
print("\tuint64_t t=tweak[0]^tweak[1];");
for i=0,okeys-1 do print("\tout["..i.."]=in["..i.."];"); end
--		for i=1,okeys do
--			print("fprintf(stderr, \"%016llX \", out["..(i-1).."]);");
--		end
--		print("fprintf(stderr, \"\\n\");");
for r=0,rounds do
	if r % 4 == 0 then
		--Add subkey.
		for i=0,okeys-1 do
			if i < okeys - 3 then
				print("\tout["..i.."]+="..keys[(subkey+i)%#keys+1]..";");
			elseif i < okeys - 1 then
				print("\tout["..i.."]+="..keys[(subkey+i)%#keys+1].."+"..
					tweaks[(subkey+i-(okeys - 3))%3+1]..";");
			else
				print("\tout["..i.."]+="..keys[(subkey+i)%#keys+1].."+"..subkey..";");
			end
		end
--		for i=1,okeys do
--			print("fprintf(stderr, \"%016llX \", out["..(i-1).."]);");
--		end
--		print("fprintf(stderr, \"\\n\");");
		subkey = subkey + 1;
	end
	if r == rounds then break; end
	for i=1,okeys-1,2 do
		print("\tout["..current_permute[i].."]=out["..current_permute[i].."]+out["..current_permute[i+1]..
			"];");
		print("\tout["..current_permute[i+1].."]=(out["..current_permute[i+1].."] << "..
			rc[(r%8)*(okeys/2)+(i+1)/2]..
			")|(out["..current_permute[i+1].."] >> "..(64-rc[(r%8)*(okeys/2)+(i+1)/2]..");"));
		print("\tout["..current_permute[i+1].."]=out["..current_permute[i].."]^out["..current_permute[i+1]..
			"];");
	end
	for i = 1,okeys do
		current_permute[i]=permutation[current_permute[i]+1];
	end
end
for i=0,okeys-1 do print("\tout["..i.."]=in["..i.."]^out["..i.."];"); end
print("\tk = 0;");
print("\tt = 0;");
print("\tasm volatile(\"\" : : \"r\"(&k) : \"memory\");");
print("\tasm volatile(\"\" : : \"r\"(&t) : \"memory\");");
print("}");
end

XGen({0,3,2,1},4,{14,16,52,57,23,40,5,37,25,33,46,12,58,22,32,32},72,"skein256_compress");
XGen({2,1,4,7,6,5,0,3},8,{46,36,19,37,33,27,14,42,17,49,36,39,44,9,54,56,39,30,34,24,13,50,10,17,25,29,39,43,8,35,
	56,22},72,"skein512_compress");
XGen({0,9,2,13,6,11,4,15,10,7,12,3,14,5,8,1},16,{24,13,8,47,8,17,22,37,38,19,10,55,49,18,23,52,33,4,51,13,34,41,59,
	17,5,20,48,41,47,28,16,25,41,9,37,31,12,47,44,30,16,34,56,51,4,53,42,41,31,44,47,46,19,42,44,25,9,48,35,52,
	23,31,37,20},80,"skein1024_compress");
