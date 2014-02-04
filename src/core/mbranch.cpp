#include "core/movie.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/mbranch.hpp"

void update_movie_state();

std::string mbranch_name(const std::string& internal)
{
	if(internal != "")
		return internal;
	return "(Default branch)";
}

std::set<std::string> mbranch_enumerate()
{
	std::set<std::string> r;
	if(!movb)
		return r;
	for(auto& i : movb.get_mfile().branches)
		r.insert(i.first);
	return r;
}

std::string mbranch_get()
{
	if(!movb)
		return "";
	return movb.get_mfile().current_branch();
}

void mbranch_set(const std::string& branch)
{
	moviefile& mf = movb.get_mfile();
	if(!movb.get_movie().readonly_mode())
		(stringfmt() << "Branches are only switchable in readonly mode.").throwex();
	if(!mf.branches.count(branch))
		(stringfmt() << "Branch '" << mbranch_name(branch) << "' does not exist.").throwex();
	if(!movb.get_movie().compatible(mf.branches[branch]))
		(stringfmt() << "Branch '" << mbranch_name(branch) << "' differs in past.").throwex();
	//Ok, execute the switch.
	mf.input = &mf.branches[branch];
	movb.get_movie().set_movie_data(mf.input);
	notify_mbranch_change();
	update_movie_state();
	messages << "Switched to branch '" << mbranch_name(branch) << "'" << std::endl;
}

void mbranch_new(const std::string& branch, const std::string& from)
{
	moviefile& mf = movb.get_mfile();
	if(mf.branches.count(branch))
		(stringfmt() << "Branch '" << mbranch_name(branch) << "' already exists.").throwex();
	mf.fork_branch(from, branch);
	messages << "Created branch '" << mbranch_name(branch) << "'" << std::endl;
	notify_mbranch_change();
}

void mbranch_rename(const std::string& oldn, const std::string& newn)
{
	moviefile& mf = movb.get_mfile();
	if(oldn == newn)
		return;
	if(!mf.branches.count(oldn))
		(stringfmt() << "Branch '" << mbranch_name(oldn) << "' does not exist.").throwex();
	if(mf.branches.count(newn))
		(stringfmt() << "Branch '" << mbranch_name(newn) << "' already exists.").throwex();
	mf.fork_branch(oldn, newn);
	if(movb.get_mfile().current_branch() == oldn) {
		mf.input = &mf.branches[newn];
		movb.get_movie().set_movie_data(mf.input);
	}
	mf.branches.erase(oldn);
	messages << "Renamed branch '" << mbranch_name(oldn) << "' to '" << mbranch_name(newn) << "'" << std::endl;
	notify_mbranch_change();
	update_movie_state();
}

void mbranch_delete(const std::string& branch)
{
	moviefile& mf = movb.get_mfile();
	if(!mf.branches.count(branch))
		(stringfmt() << "Branch '" << mbranch_name(branch) << "' does not exist.").throwex();
	if(movb.get_mfile().current_branch() == branch)
		(stringfmt() << "Can't delete current branch '" << mbranch_name(branch) << "'.").throwex();
	movb.get_mfile().branches.erase(branch);
	messages << "Deleted branch '" << mbranch_name(branch) << "'" << std::endl;
	notify_mbranch_change();
}

std::set<std::string> mbranch_movie_branches(const std::string& filename)
{
	moviefile::branch_extractor e(filename);
	return e.enumerate();
}

void mbranch_import(const std::string& filename, const std::string& ibranch, const std::string& branchname, int mode)
{
	auto& mv = movb.get_mfile();
	if(mv.branches.count(branchname) && &mv.branches[branchname] == mv.input)
		(stringfmt() << "Can't overwrite current branch.").throwex();

	controller_frame_vector v(mv.input->get_types());
	if(mode == MBRANCH_IMPORT_TEXT || mode == MBRANCH_IMPORT_BINARY) {
		std::ifstream file(filename, (mode == MBRANCH_IMPORT_BINARY) ? std::ios_base::binary :
			std::ios_base::in);
		if(!file)
			(stringfmt() << "Can't open '" << filename << "' for reading.").throwex();
		controller_frame_vector::notify_freeze freeze(v);
		if(mode == MBRANCH_IMPORT_BINARY) {
			uint64_t stride = v.get_stride();
			uint64_t pageframes = v.get_frames_per_page();
			uint64_t vsize = 0;
			size_t pagenum = 0;
			size_t pagesize = stride * pageframes;
			while(file) {
				v.resize(vsize + pageframes);
				unsigned char* contents = v.get_page_buffer(pagenum++);
				file.read(reinterpret_cast<char*>(contents), pagesize);
				vsize += (file.gcount() / stride);
			}
			v.resize(vsize);
		} else {
			std::string line;
			controller_frame tmpl = v.blank_frame(false);
			while(file) {
				std::getline(file, line);
				istrip_CR(line);
				if(line.length() == 0)
					continue;
				tmpl.deserialize(line.c_str());
				v.append(tmpl);
			}
		}
	} else if(mode == MBRANCH_IMPORT_MOVIE) {
		moviefile::branch_extractor e(filename);
		e.read(ibranch, v);
	}
	bool created = !mv.branches.count(branchname);
	try {
		mv.branches[branchname] = v;
	} catch(...) {
		if(created)
			mv.branches.erase(branchname);
	}
}

void mbranch_export(const std::string& filename, const std::string& branchname, bool binary)
{
	auto& mv = movb.get_mfile();
	if(!mv.branches.count(branchname))
		(stringfmt() << "Branch '" << branchname << "' does not exist.").throwex();
	auto& v = mv.branches[branchname];
	std::ofstream file(filename, binary ? std::ios_base::binary : std::ios_base::out);
	if(!file)
		(stringfmt() << "Can't open '" << filename << "' for writing.").throwex();
	if(binary) {
		uint64_t pages = v.get_page_count();
		uint64_t stride = v.get_stride();
		uint64_t pageframes = v.get_frames_per_page();
		uint64_t vsize = v.size();
		size_t pagenum = 0;
		while(vsize > 0) {
			uint64_t count = (vsize > pageframes) ? pageframes : vsize;
			size_t bytes = count * stride;
			unsigned char* content = v.get_page_buffer(pagenum++);
			file.write(reinterpret_cast<char*>(content), bytes);
			vsize -= count;
		}
	} else {
		char buf[MAX_SERIALIZED_SIZE];
		for(uint64_t i = 0; i < v.size(); i++) {
			v[i].serialize(buf);
			file << buf << std::endl;
		}
	}
	if(!file)
		(stringfmt() << "Can't write to '" << filename << "'.").throwex();
}
