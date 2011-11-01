name = io.stdin:read("*l");
print("extern \"C\" {");
print("#include <linux/input.h>");
print("}");
print("void " .. name .. "(const char** x) {");
for line in io.stdin:lines() do
a,b = string.match(line, "(%S+)%s+(.*)");
print("#ifdef " .. a);
print("x[" .. a .. "] = \"" .. b .. "\";");
print("#endif");
end
print("}");
