#include <map>
#include <string>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iomanip>

std::map<uint32_t, uint32_t> glyph_index;
std::map<std::string, uint32_t> glyph_offsets;
std::map<uint32_t, std::string> glyphs;
uint32_t next_offset = 1;

void add_glyph(uint32_t codepoint, const std::string& appearence)
{
	if(((codepoint >> 16) > 10) || ((codepoint & 0x1FF800) == 0xD800)) {
		std::cerr << "Illegal codepoint " << std::hex << codepoint << "." << std::endl;
		exit(1);
	}
	if(!glyph_offsets.count(appearence)) {
		if(appearence.find_first_not_of("0123456789ABCDEFabcdef") < appearence.length()) {
			std::cerr << "Invalid font representation (invalid hex char)." << std::endl;
			std::cerr << "Faulty representation: '" << appearence << "'." << std::endl;
			exit(1);
		}
		glyph_offsets[appearence] = next_offset;
		glyph_index[next_offset] = codepoint;
		if(appearence.length() == 32)
			next_offset += 5;
		else if(appearence.length() == 64)
			next_offset += 9;
		else {
			std::cerr << "Invalid font representation (length " << appearence.length() << ")."
				<< std::endl;
			std::cerr << "Faulty representation: '" << appearence << "'." << std::endl;
			exit(1);
		}
	}
	glyphs[codepoint] = appearence;
}

uint32_t bucket_size(uint32_t items)
{
	if(items <= 4)
		return items;
	return static_cast<uint32_t>(pow(items, 1.5));
}

//This is Jenkin's MIX function.
uint32_t keyhash(uint32_t key, uint32_t item, uint32_t mod)
{
	uint32_t a = key;
	uint32_t b = 0;
	uint32_t c = item;
	a=a-b;	a=a-c;	a=a^(c >> 13);
	b=b-c;	b=b-a;	b=b^(a << 8);
	c=c-a;	c=c-b;	c=c^(b >> 13);
	a=a-b;	a=a-c;	a=a^(c >> 12);
	b=b-c;	b=b-a;	b=b^(a << 16);
	c=c-a;	c=c-b;	c=c^(b >> 5);
	a=a-b;	a=a-c;	a=a^(c >> 3);
	b=b-c;	b=b-a;	b=b^(a << 10);
	c=c-a;	c=c-b;	c=c^(b >> 15);
	return c % mod;
}

uint32_t wrong_key(uint32_t key, uint32_t hash, uint32_t mod)
{
	uint32_t i = 0;
	if(mod <= 1)
		return 0;
	while(keyhash(key, i, mod) == hash)
		i++;
	return i;
}

std::pair<uint32_t, uint32_t> make_subdirectory(std::vector<uint32_t>& items)
{
	if(items.size() < 2)
		return std::make_pair(0, items.size());
	std::vector<uint32_t> memory;
	memory.resize(bucket_size(items.size()));
	uint32_t seed = 1;
	unsigned tries = 0;
	while(true) {
		//Safety hatch: If unsuccessful too many times, increase the hash size.
		if(tries == 100) {
			memory.resize(memory.size() + 1);
			tries = 0;
		}
		bool success = true;
		seed = rand();
		for(uint32_t i = 0; i < memory.size(); i++)
			memory[i] = 0xFFFFFFFFUL;
		for(uint32_t i = 0; i < items.size(); i++) {
			uint32_t j = keyhash(seed, items[i], memory.size());
			if(memory[j] != 0xFFFFFFFFUL) {
				success = false;
				break;
			}
			memory[j] = items[i];
		}
		if(success)
			break;
		tries++;
	}
	return std::make_pair(seed, memory.size());
}

void write_subdirectory(std::vector<uint32_t>& items, std::pair<uint32_t, uint32_t> p,
	uint32_t badkey)
{
	if(!p.second)
		return;
	std::vector<uint32_t> memory;
	std::cout << "," << p.first << "UL," << p.second << "UL";
	memory.resize(p.second);
	for(uint32_t i = 0; i < memory.size(); i++)
		memory[i] = badkey;
	for(uint32_t i = 0; i < items.size(); i++)
		memory[keyhash(p.first, items[i], memory.size())] = items[i];
	for(uint32_t i = 0; i < memory.size(); i++) {
		if(memory[i] != badkey)
			std::cout << "," << memory[i] << "UL," << glyph_offsets[glyphs[memory[i]]] << "UL";
		else
			std::cout << "," << badkey << "UL,0UL";
	}
	std::cout << std::endl;
}

uint32_t glyph_part_string(const std::string& str)
{
	return strtoul(str.c_str(), NULL, 16);
}

void write_glyphs()
{
	for(std::map<uint32_t, uint32_t>::iterator i = glyph_index.begin(); i != glyph_index.end(); ++i) {
		std::string glyph = glyphs[i->second];
		if(glyph.length() == 32) {
			std::cout << ",0";
			for(unsigned i = 0; i < 32; i += 8)
				std::cout << "," << glyph_part_string(glyph.substr(i, 8)) << "UL";
		} else if(glyph.length() == 64) {
			std::cout << ",1";
			for(unsigned i = 0; i < 64; i += 8)
				std::cout << "," << glyph_part_string(glyph.substr(i, 8)) << "UL";
		} else {
			std::cerr << "INTERNAL ERROR: Invalid glyph data" << std::endl;
			exit(1);
		}
	}
	std::cout << std::endl;
}

void write_main_directory(uint32_t seed, std::vector<std::vector<uint32_t> >& main_directory,
	std::vector<std::pair<uint32_t, uint32_t> >& subdirectories)
{
	std::cout << "," << seed << "UL";
	std::cout << "," << main_directory.size() << "UL";
	uint32_t subdir_offset = next_offset + 4 + main_directory.size();
	for(size_t i = 0; i < main_directory.size(); i++) {
		if(subdirectories[i].second) {
			std::cout << "," << subdir_offset << "UL";
			subdir_offset = subdir_offset + 2 + 2 * subdirectories[i].second;
		} else
			std::cout << "," << next_offset << "UL";
	}
	std::cout << std::endl;
}


int main()
{
	std::string line;
	srand(time(NULL));
	while(std::getline(std::cin, line)) {
		if(line[line.length() - 1] == '\r')
			line = line.substr(0, line.length() - 1);
		size_t split = line.find_first_of("#:");
		if(split > line.length() || line[split] == '#')
			continue;
		std::string codepoint = line.substr(0, split);
		std::string appearence = line.substr(split + 1);
		uint32_t cp = 0;
		char* end;
		cp = strtoul(codepoint.c_str(), &end, 16);
		if(*end) {
			std::cerr << "Invalid codepoint number '" << codepoint << "." << std::endl;
			exit(1);
		}
		add_glyph(cp, appearence);
	}
	std::cerr << "Loaded " << glyphs.size() << " glyphs (" << glyph_offsets.size() << " distinct)." << std::endl;
	if(!glyphs.size()) {
		std::cerr << "Need at least 1 glyph." << std::endl;
		return 1;
	}
	uint32_t best_seed = 0;
	uint32_t best_score = 0xFFFFFFFFUL;
	uint32_t main_directory_size = glyphs.size();
	for(unsigned i = 0; i < 1000; i++) {
		if(i % 10 == 0)
			std::cerr << "." << std::flush;
		uint32_t seed = rand();
		std::vector<uint32_t> bucket_items;
		bucket_items.resize(main_directory_size);
		for(uint32_t i = 0; i < main_directory_size; i++)
			bucket_items[i] = 0;
		for(std::map<uint32_t, std::string>::iterator i = glyphs.begin(); i != glyphs.end(); ++i)
			bucket_items[keyhash(seed, i->first, main_directory_size)]++;
		uint32_t score = 0;
		for(uint32_t i = 0; i < main_directory_size; i++)
			if(bucket_items[i])
				score = score + 2 * bucket_size(bucket_items[i]) + 3;
			else
				score = score + 1;
		if(score < best_score) {
			best_seed = seed;
			best_score = score;
		}
	}
	std::cerr << std::endl;
	std::cerr << "Best estimated score: " << best_score << " for seed " << best_seed << "." << std::endl;
	std::vector<std::vector<uint32_t> > main_directory;
	std::vector<std::pair<uint32_t, uint32_t> > subdirectories;
	main_directory.resize(main_directory_size);
	subdirectories.resize(main_directory_size);
	for(std::map<uint32_t, std::string>::iterator i = glyphs.begin(); i != glyphs.end(); ++i)
		main_directory[keyhash(best_seed, i->first, main_directory_size)].push_back(i->first);
	uint32_t update_interval = main_directory_size / 10;
	unsigned real_score = 0;
	for(size_t i = 0; i < main_directory.size(); i++) {
		if((i % update_interval) == 0)
			std::cerr << "." << std::flush;
		subdirectories[i] = make_subdirectory(main_directory[i]);
		if(subdirectories[i].second)
			real_score = real_score + 2 * subdirectories[i].second + 3;
		else
			real_score = real_score + 1;
	}
	std::cerr << std::endl;
	std::cerr << "Final score: " << real_score << " (" << next_offset + real_score + 4 << " elements)."
		<< std::endl;
	std::cout << "#include <cstdint>" << std::endl;
	std::cout << "uint32_t fontdata_size = " << next_offset + real_score + 4 << ";" << std::endl;
	std::cout << "uint32_t fontdata[] = {";
	std::cout << next_offset + 2 << "UL";
	write_glyphs();
	std::cout << ",0,0" << std::endl;
	write_main_directory(best_seed, main_directory, subdirectories);
	for(size_t i = 0; i < main_directory.size(); i++)
		write_subdirectory(main_directory[i], subdirectories[i], wrong_key(best_seed, i, main_directory_size));
	std::cout << "};" << std::endl;
	return 0;
}
