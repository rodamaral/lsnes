#include "cmdhelp/lua.hpp"
#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/project.hpp"
#include "core/queue.hpp"
#include "core/rom.hpp"
#include "core/ui-services.hpp"
#include "library/keyboard.hpp"
#include <functional>

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

	void update_dumperinfo(emulator_instance& inst, std::map<std::string, dumper_information_1>& new_dumpers,
		dumper_factory_base* d)
	{
		struct dumper_information_1 inf;
		inf.factory = d;
		inf.name = d->name();
		std::set<std::string> mset = d->list_submodes();
		for(auto i : mset)
			inf.modes[i] = d->modename(i);
		inf.active = inst.mdumper->busy(d);
		inf.hidden = d->hidden();
		new_dumpers[d->id()] = inf;
	}
}

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
	auto supdater = inst.supdater;
	inst.iqueue->run_async([project, supdater, id, name]() {
		auto p = project->get();
		if(!p) return;
		p->set_branch_name(id, name);
		p->flush();
		supdater->update();
	}, onerror);
}

void UI_reparent_branch(emulator_instance& inst, uint64_t id, uint64_t pid,
	std::function<void(std::exception&)> onerror)
{
	auto project = inst.project;
	auto supdater = inst.supdater;
	inst.iqueue->run_async([project, supdater, id, pid]() {
		auto p = project->get();
		if(!p) return;
		p->set_parent_branch(id, pid);
		p->flush();
		supdater->update();
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
	auto supdater = inst.supdater;
	inst.iqueue->run_async([project, supdater, id]() {
		auto p = project->get();
		if(!p) return;
		p->set_current_branch(id);
		p->flush();
		supdater->update();
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
			inst.supdater->update();
			inst.dispatch->title_change();
		} else {
			inst.mlogic->get_mfile().gamename = info.gamename;
			inst.mlogic->get_mfile().authors = _authors;
			set_mprefix_for_project(info.prefix);
		}
		if(proj && info.autorunlua)
			for(auto i : info.luascripts)
				if(!oldscripts.count(i))
					inst.command->invoke(CLUA::run.name, i);
	});
}

dumper_information UI_get_dumpers(emulator_instance& inst)
{
	dumper_information x;
	inst.iqueue->run([&inst, &x]() {
		std::set<dumper_factory_base*> dset = dumper_factory_base::get_dumper_set();
		for(auto i : dset)
			update_dumperinfo(inst, x.dumpers, i);
	});
	return x;
}

void UI_start_dump(emulator_instance& inst, dumper_factory_base& factory, const std::string& mode,
	const std::string& prefix)
{
	inst.iqueue->run([&inst, &factory, mode, prefix]() {
		inst.mdumper->start(factory, mode, prefix);
	});
}

void UI_end_dump(emulator_instance& inst, dumper_factory_base& factory)
{
	inst.iqueue->run([&inst, &factory]() {
		auto in = inst.mdumper->get_instance(&factory);
		delete in;
	});
}

void UI_do_keypress(emulator_instance& inst, const keyboard::modifier_set& mods, keyboard::key_key& key,
	bool polarity)
{
	auto _key = &key;
	inst.iqueue->run_async([mods, _key, polarity]() {
		_key->set_state(mods, polarity ? 1 : 0);
	}, [](std::exception& e) {});
}

bool UI_has_movie(emulator_instance& inst)
{
	bool ret = false;
	inst.iqueue->run([&inst, &ret]() {
		ret = !!*inst.mlogic && !inst.rom->isnull();
	});
	return ret;
}

void UI_save_movie(emulator_instance& inst, std::ostringstream& stream)
{
	inst.iqueue->run([&inst, &stream]() {
		auto prj = inst.project->get();
		if(prj) {
			inst.mlogic->get_mfile().gamename = prj->gamename;
			inst.mlogic->get_mfile().authors = prj->authors;
		}
		inst.mlogic->get_mfile().dyn.active_macros.clear();
		inst.mlogic->get_mfile().save(stream, inst.mlogic->get_rrdata(), false);
	});
}

std::pair<std::string, std::string> UI_lookup_platform_and_game(emulator_instance& inst)
{
	std::string plat;
	std::string game;
	inst.iqueue->run([&inst, &plat, &game]() {
		auto prj = inst.project->get();
		if(prj)
			game = prj->gamename;
		else
			game = inst.mlogic->get_mfile().gamename;
		plat = lookup_sysregion_mapping(inst.mlogic->get_mfile().gametype->get_name());
	});
	return std::make_pair(plat, game);
}

std::string UI_get_project_otherpath(emulator_instance& inst)
{
	std::string path;
	inst.iqueue->run([&inst, &path]() {
		path = inst.project->otherpath();
	});
	return path;
}

std::string UI_get_project_moviepath(emulator_instance& inst)
{
	std::string path;
	inst.iqueue->run([&inst, &path]() {
		path = inst.project->moviepath();
	});
	return path;
}

bool UI_in_project_context(emulator_instance& inst)
{
	bool pc;
	inst.iqueue->run([&inst, &pc]() {
		pc = (inst.project->get() != NULL);
	});
	return pc;
}
