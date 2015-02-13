#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/emustatus.hpp"
#include "core/mbranch.hpp"
#include "core/messages.hpp"
#include "core/movie.hpp"
#include "library/string.hpp"

movie_branches::movie_branches(movie_logic& _mlogic, emulator_dispatch& _dispatch, status_updater& _supdater)
	: mlogic(_mlogic), edispatch(_dispatch), supdater(_supdater)
{
}

std::string movie_branches::name(const std::string& internal)
{
	if(internal != "")
		return internal;
	return "(Default branch)";
}

std::set<std::string> movie_branches::enumerate()
{
	std::set<std::string> r;
	if(!mlogic)
		return r;
	for(auto& i : mlogic.get_mfile().branches)
		r.insert(i.first);
	return r;
}

std::string movie_branches::get()
{
	if(!mlogic)
		return "";
	return mlogic.get_mfile().current_branch();
}

void movie_branches::set(const std::string& branch)
{
	moviefile& mf = mlogic.get_mfile();
	if(!mlogic.get_movie().readonly_mode())
		(stringfmt() << "Branches are only switchable in readonly mode.").throwex();
	if(!mf.branches.count(branch))
		(stringfmt() << "Branch '" << name(branch) << "' does not exist.").throwex();
	if(!mlogic.get_movie().compatible(mf.branches[branch]))
		(stringfmt() << "Branch '" << name(branch) << "' differs in past.").throwex();
	//Ok, execute the switch.
	mf.input = &mf.branches[branch];
	mlogic.get_movie().set_movie_data(mf.input);
	edispatch.mbranch_change();
	supdater.update();
	messages << "Switched to branch '" << name(branch) << "'" << std::endl;
}

void movie_branches::_new(const std::string& branch, const std::string& from)
{
	moviefile& mf = mlogic.get_mfile();
	if(mf.branches.count(branch))
		(stringfmt() << "Branch '" << name(branch) << "' already exists.").throwex();
	mf.fork_branch(from, branch);
	messages << "Created branch '" << name(branch) << "'" << std::endl;
	edispatch.mbranch_change();
}

void movie_branches::rename(const std::string& oldn, const std::string& newn)
{
	moviefile& mf = mlogic.get_mfile();
	if(oldn == newn)
		return;
	if(!mf.branches.count(oldn))
		(stringfmt() << "Branch '" << name(oldn) << "' does not exist.").throwex();
	if(mf.branches.count(newn))
		(stringfmt() << "Branch '" << name(newn) << "' already exists.").throwex();
	mf.fork_branch(oldn, newn);
	if(mlogic.get_mfile().current_branch() == oldn) {
		mf.input = &mf.branches[newn];
		mlogic.get_movie().set_movie_data(mf.input);
	}
	mf.branches.erase(oldn);
	messages << "Renamed branch '" << name(oldn) << "' to '" << name(newn) << "'" << std::endl;
	edispatch.mbranch_change();
	supdater.update();
}

void movie_branches::_delete(const std::string& branch)
{
	moviefile& mf = mlogic.get_mfile();
	if(!mf.branches.count(branch))
		(stringfmt() << "Branch '" << name(branch) << "' does not exist.").throwex();
	if(mlogic.get_mfile().current_branch() == branch)
		(stringfmt() << "Can't delete current branch '" << name(branch) << "'.").throwex();
	mlogic.get_mfile().branches.erase(branch);
	messages << "Deleted branch '" << name(branch) << "'" << std::endl;
	edispatch.mbranch_change();
}

std::set<std::string> movie_branches::_movie_branches(const std::string& filename)
{
	moviefile::branch_extractor e(filename);
	return e.enumerate();
}

void movie_branches::import_branch(const std::string& filename, const std::string& ibranch,
	const std::string& branchname, int mode)
{
	auto& mv = mlogic.get_mfile();
	if(mv.branches.count(branchname) && &mv.branches[branchname] == mv.input)
		(stringfmt() << "Can't overwrite current branch.").throwex();

	portctrl::frame_vector v(mv.input->get_types());
	if(mode == MBRANCH_IMPORT_TEXT || mode == MBRANCH_IMPORT_BINARY) {
		std::ifstream file(filename, (mode == MBRANCH_IMPORT_BINARY) ? std::ios_base::binary :
			std::ios_base::in);
		if(!file)
			(stringfmt() << "Can't open '" << filename << "' for reading.").throwex();
		portctrl::frame_vector::notify_freeze freeze(v);
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
			v.recount_frames();
		} else {
			std::string line;
			portctrl::frame tmpl = v.blank_frame(false);
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

void movie_branches::export_branch(const std::string& filename, const std::string& branchname, bool binary)
{
	auto& mv = mlogic.get_mfile();
	if(!mv.branches.count(branchname))
		(stringfmt() << "Branch '" << branchname << "' does not exist.").throwex();
	auto& v = mv.branches[branchname];
	std::ofstream file(filename, binary ? std::ios_base::binary : std::ios_base::out);
	if(!file)
		(stringfmt() << "Can't open '" << filename << "' for writing.").throwex();
	if(binary) {
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
