#include "streamcompress.hpp"
#include "string.hpp"
#include <stdexcept>

namespace
{
	std::map<std::string, std::function<stream_compressor_base*(const std::string&)>>& compressors()
	{
		static std::map<std::string, std::function<stream_compressor_base*(const std::string&)>> x;
		return x;
	}
}

stream_compressor_base::~stream_compressor_base()
{
}

std::set<std::string> stream_compressor_base::get_compressors()
{
	std::set<std::string> r;
	for(auto& i : compressors())
		r.insert(i.first);
	return r;
}

stream_compressor_base* stream_compressor_base::create_compressor(const std::string& name,
	const std::string& args)
{
	if(!compressors().count(name))
		throw std::runtime_error("No such compressor");
	compressors()[name](args);
}

void stream_compressor_base::do_register(const std::string& name,
	std::function<stream_compressor_base*(const std::string&)> ctor)
{
	compressors()[name] = ctor;
}

void stream_compressor_base::do_unregister(const std::string& name)
{
	compressors().erase(name);
}

std::map<std::string, std::string> stream_compressor_parse_attributes(const std::string& val)
{
	std::map<std::string, std::string> r;
	std::string v = val;
	while(v != "") {
		auto x = regex("([^=]+)=([^,]+)(,(.*)|$)", v);
		r[x[1]] = x[2];
		v = x[4];
	}
	return r;
}
