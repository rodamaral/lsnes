#pragma once

void handle_sram_load(lsnes_core_load_sram& arg, const std::string& savename, unsigned char* rdata, size_t rsize,
	std::set<std::string>& used)
{
	lsnes_core_sram* sram = NULL;
	for(lsnes_core_sram* i = arg.srams; i->name; i++)  {
		if(savename == i->name)
			sram = i;
	}
	if(sram) {
		if(rsize != sram->size) {
			(message_output() << "WARNING: SRAM '" << savename << "': Loaded "
				<< sram->size << " bytes, but the SRAM is " << rsize << "."
				<< std::endl).end();
		}
		memcpy(rdata, sram->data, (rsize < sram->size) ? rsize : sram->size);
		if(rsize > sram->size)
			memset(rdata + sram->size, 0, rsize - sram->size);
		used.insert(sram->name);
	} else {
		(message_output() << "WARNING: SRAM '" << savename << "': No data."
			<< std::endl).end();
	}
}

void add_sram_entry(std::vector<std::pair<const char*, std::pair<const char*, size_t>>>& out, unsigned& key,
	const std::string& name, const void* data, size_t datalen)
{
	const char* _name = tmpalloc_str(key++, name);
	const char* buf = tmpalloc_data(key++, data, datalen);
	out.push_back(std::make_pair(_name, std::make_pair(buf, datalen)));
}

std::string sram_name(const nall::string& _id, SNES::Cartridge::Slot slotname)
{
	std::string id(_id, _id.length());
	return id.substr(1);
}

