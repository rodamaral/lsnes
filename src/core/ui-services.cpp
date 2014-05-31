#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/queue.hpp"
#include "core/ui-services.hpp"

namespace
{
	void fill_namemap(project_info& p, uint64_t id, std::map<uint64_t, std::string>& namemap,
		std::map<uint64_t, std::set<uint64_t>>& childmap)
	{
		namemap[id] = p.get_branch_name(id);
		auto s = p.branch_children(id);
		for(auto i : s)
			fill_namemap(p, i, namemap, childmap);
		childmap[id] = s;
	}
}

void update_movie_state();
void do_flush_slotinfo();

void UI_get_branch_map(emulator_instance& inst, uint64_t& cur, std::map<uint64_t, std::string>& namemap,
	std::map<uint64_t, std::set<uint64_t>>& childmap)
{
	auto project = inst.project;
	inst.iqueue->run([project, &cur, &namemap, &childmap]() {
		auto p = project->get();
		if(!p) return;
		fill_namemap(*p, 0, namemap, childmap);
		cur = p->get_current_branch();
	});
}

void UI_call_flush(emulator_instance& inst, std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	inst.iqueue->run_async([project]() {
		auto p = project->get();
		if(p) p->flush();
	}, onerror);
}

void UI_create_branch(emulator_instance& inst, uint64_t id, const std::string& name,
	std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	inst.iqueue->run_async([project, id, name]() {
		auto p = project->get();
		if(!p) return;
		p->create_branch(id, name);
		p->flush();
	}, onerror);
}

void UI_rename_branch(emulator_instance& inst, uint64_t id, const std::string& name,
	std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	inst.iqueue->run_async([project, id, name]() {
		auto p = project->get();
		if(!p) return;
		p->set_branch_name(id, name);
		p->flush();
		update_movie_state();
	}, onerror);
}

void UI_reparent_branch(emulator_instance& inst, uint64_t id, uint64_t pid,
	std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	inst.iqueue->run_async([project, id, pid]() {
		auto p = project->get();
		if(!p) return;
		p->set_parent_branch(id, pid);
		p->flush();
		update_movie_state();
	}, onerror);
}

void UI_delete_branch(emulator_instance& inst, uint64_t id, std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	inst.iqueue->run_async([project, id]() {
		auto p = project->get();
		if(!p) return;
		p->delete_branch(id);
		p->flush();
	}, onerror);
}

void UI_switch_branch(emulator_instance& inst, uint64_t id, std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	inst.iqueue->run_async([project, id]() {
		auto p = project->get();
		if(!p) return;
		p->set_current_branch(id);
		p->flush();
		update_movie_state();
	}, onerror);
}

project_author_info UI_load_author_info(emulator_instance& inst)
{
	project_author_info x;
	inst.iqueue->run([&inst, &x]() {
		project_info* proj = inst.project->get();
		x.is_project = (proj != NULL);
		x.autorunlua = false;
		if(proj) {
			x.projectname = proj->name;
			x.directory = proj->directory;
			x.prefix = proj->prefix;
			x.luascripts = proj->luascripts;
			x.gamename = proj->gamename;
			for(auto i : proj->authors)
				x.authors.push_back(i);
		} else {
			x.prefix = get_mprefix_for_project();
			x.gamename = inst.mlogic->get_mfile().gamename;
			for(auto i : inst.mlogic->get_mfile().authors)
				x.authors.push_back(i);
		}
	});
	return x;
}

void UI_save_author_info(emulator_instance& inst, project_author_info& info)
{
	inst.iqueue->run([&inst, info]() {
		project_info* proj = inst.project->get();
		std::set<std::string> oldscripts;
		std::vector<std::pair<std::string, std::string>> _authors(info.authors.begin(), info.authors.end());
		if(proj) {
			for(auto i : proj->luascripts)
				oldscripts.insert(i);
			proj->gamename = info.gamename;
			proj->authors = _authors;
			proj->prefix = info.prefix;
			proj->directory = info.directory;
			proj->name = info.projectname;
			proj->luascripts = info.luascripts;
			proj->flush();
			//For save status to immediately update.
			do_flush_slotinfo();
			update_movie_state();
			inst.dispatch->title_change();
		} else {
			inst.mlogic->get_mfile().gamename = info.gamename;
			inst.mlogic->get_mfile().authors = _authors;
			set_mprefix_for_project(info.prefix);
		}
		if(proj && info.autorunlua)
			for(auto i : info.luascripts)
				if(!oldscripts.count(i))
					inst.command->invoke("run-lua " + i);
	});
}