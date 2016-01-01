#include "core/instance.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "library/directory.hpp"
#include "library/zip.hpp"

#include <fstream>

namespace
{
	bool db_loaded;
	std::map<std::pair<std::string, uint64_t>, std::string> our_db;
	std::string database_name()
	{
		return get_config_path() + "/rom.db";
	}

	void load_db()
	{
		std::ifstream db(database_name());
		if(db) {
			std::string line;
			while(std::getline(db, line)) {
				istrip_CR(line);
				size_t split = line.find_first_of("|");
				if(split >= line.length())
					continue;
				std::string hash = line.substr(0, split);
				std::string filename = line.substr(split + 1);
				uint64_t prefix = 0;
				size_t split2 = hash.find_first_of(":");
				if(split2 < hash.length()) {
					std::string _prefix = hash.substr(split2 + 1);
					hash = hash.substr(0, split2);
					try { prefix = parse_value<uint64_t>(_prefix); } catch(...) {};
				}
				if(hash != "") {
					our_db[std::make_pair(filename, prefix)] = hash;
				} else {
					our_db.erase(std::make_pair(filename, prefix));
				}
			}
		}
		db_loaded = true;
	}

	void record_hash(const std::string& _file, uint64_t prefix, const std::string& hash)
	{
		if(!db_loaded) load_db();
		//Database write. If there is existing entry for file, it is overwritten.
		std::string file = directory::absolute_path(_file);
		std::pair<std::string, uint64_t> key = std::make_pair(file, prefix);
		if(hash == "" && !our_db.count(key))
			return;		//Already correct.
		if(our_db.count(key) && our_db[key] == hash)
			return;		//Already correct.
		if(hash != "")
			our_db[key] = hash;
		else
			our_db.erase(key);
		std::ofstream db(database_name(), std::ios::app);
		db << hash << ":" << prefix << "|" << file << std::endl;
	}

	void record_hash_deleted(const std::string& _file)
	{
		if(!db_loaded) load_db();
		auto itr = our_db.begin();
		while(true) {
			itr = our_db.lower_bound(std::make_pair(_file, 0));
			if(itr == our_db.end() || itr->first.first != _file)
				return;
			record_hash(_file, itr->first.second, "");
		}
	}

	std::list<std::pair<std::string, uint64_t>> retretive_files_by_hash(const std::string& hash)
	{
		if(!db_loaded) load_db();
		//Database read. The read is for keys with given value.
		std::list<std::pair<std::string, uint64_t>> x;
		for(auto i : our_db)
			if(i.second == hash) {
				x.push_back(i.first);
			}
		return x;
	}

	std::string hash_file(const std::string& file, uint64_t hsize)
	{
		if(!zip::file_exists(file)) {
			record_hash_deleted(file);
			return "";
		}
		try {
			fileimage::hashval f = lsnes_image_hasher(file, fileimage::std_headersize_fn(hsize));
			std::string hash = f.read();
			uint64_t prefix = f.prefix();
			record_hash(file, prefix, hash);
			return hash;
		} catch(std::exception& e) {
			return "";	//Error.
		}
	}

	std::string try_basename(const std::string& hash, const std::string& xhash,
		const std::string& file, uint64_t headersize)
	{
		if(!zip::file_exists(file))
			return "";
		std::string xfile = file + ".xml";
		bool has_xfile = zip::file_exists(xfile);
		if(xhash == "" && has_xfile)
			return "";	//Markup mismatch.
		if(xhash != "" && !has_xfile)
			return "";	//Markup mismatch.
		if(has_xfile && hash_file(xfile, 0) != xhash)
			return "";	//Markup mismatch.
		if(hash_file(file, headersize) == hash)
			return file;
		return "";
	}

	std::string try_scan_hint_dir(const std::string& hint, const std::string& hash,
		const std::string& xhash, const std::string& dir, const std::set<std::string>& extensions,
		uint64_t headersize)
	{
		std::string x;
		for(auto j : extensions)
			if((x = try_basename(hash, xhash, dir + "/" + hint + "." + j, headersize)) != "")
				return x;
		return "";
	}

	std::string try_guess_rom_core(const std::string& hint, const std::string& hash, const std::string& xhash,
		const std::set<std::string>& extensions, uint64_t headersize, bool bios)
	{
		auto& core = CORE();
		std::string x;
		std::string romdir = SET_rompath(*core.settings);
		std::string biosdir = SET_firmwarepath(*core.settings);
		if((x = try_scan_hint_dir(hint, hash, xhash, romdir, extensions, headersize)) != "") return x;
		if(bios && (x = try_scan_hint_dir(hint, hash, xhash, biosdir, extensions, headersize)) != "")
			return x;
		for(auto j : retretive_files_by_hash(hash)) {
			if((x = try_basename(hash, xhash, j.first, headersize)) != "")
				return x;
		}
		return "";
	}

	void try_guess_rom(rom_request& req, unsigned i)
	{
		std::string x;
		if(req.guessed[i] || !req.has_slot[i] || req.hash[i] == "")
			return;  //Stay away from nonexistent slots and already guessed ones.

		std::set<std::string> extensions;
		uint64_t header = 0;
		for(auto j : req.cores) {
			if(j->get_image_count() <= i)
				continue;
			for(auto k : j->get_image_info(i).extensions) {
				extensions.insert(k);
			}
			header = j->get_image_info(i).headersize;
		}

		if((x = try_guess_rom_core(req.filename[i], req.hash[i], req.hashxml[i], extensions, header,
			i == 0)) != "") {
			req.filename[i] = x;
			req.guessed[i] = true;
		}
	}
}

std::string try_to_guess_rom(const std::string& hint, const std::string& hash, const std::string& xhash,
	core_type& type, unsigned i)
{
	std::set<std::string> extensions;
	if(type.get_image_count() <= i)
		return "";
	for(auto k : type.get_image_info(i).extensions)
		extensions.insert(k);
	uint64_t header = type.get_image_info(i).headersize;
	return try_guess_rom_core(hint, hash, xhash, extensions, header, type.get_biosname() != "" && i == 0);
}

void try_guess_roms(rom_request& req)
{
	try {
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++)
			try_guess_rom(req, i);
	} catch(std::exception& e) {
	}
}

void record_filehash(const std::string& file, uint64_t prefix, const std::string& hash)
{
	record_hash(file, prefix, hash);
}
