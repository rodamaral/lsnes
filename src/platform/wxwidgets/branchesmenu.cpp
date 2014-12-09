#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/settings-keyentry.hpp"
#include "platform/wxwidgets/menu_branches.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include "core/debug.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/project.hpp"
#include "core/moviedata.hpp"
#include "core/ui-services.hpp"

namespace
{
	//Tree of branches.
	class branches_tree : public wxTreeCtrl
	{
	public:
		branches_tree(wxWindow* parent, emulator_instance& _inst, int id, bool _nosels)
			: wxTreeCtrl(parent, id), inst(_inst), nosels(_nosels)
		{
			CHECK_UI_THREAD;
			SetMinSize(wxSize(400, 300));
			branchchange.set(inst.dispatch->branch_change, [this]() { runuifun([this]() {
				this->update(); }); });
			update();
		}
		struct selection
		{
			wxTreeItemId item;
			bool isroot;
			bool haschildren;
			bool iscurrent;
			uint64_t id;
			std::string name;
		};
		selection get_selection()
		{
			CHECK_UI_THREAD;
			selection s;
			s.item = GetSelection();
			for(auto i : ids) {
				if(s.item.IsOk() && i.second == s.item) {
					s.isroot = (i.first == 0);
					s.haschildren = with_children.count(i.first);
					s.id = i.first;
					s.iscurrent = (i.first == current);
					return s;
				}
			}
			s.isroot = false;
			s.haschildren = false;
			s.iscurrent = false;
			s.id = 0xFFFFFFFFFFFFFFFFULL;
			return s;
		}
		void update()
		{
			CHECK_UI_THREAD;
			std::map<uint64_t, std::string> namemap;
			std::map<uint64_t, std::set<uint64_t>> childmap;
			uint64_t cur = 0;
			UI_get_branch_map(inst, cur, namemap, childmap);
			current = cur;
			selection cursel = get_selection();
			std::set<uint64_t> expanded;
			for(auto i : ids)
				if(IsExpanded(i.second))
					expanded.insert(i.first);
			DeleteAllItems();
			ids.clear();
			with_children.clear();
			if(namemap.empty()) return;
			//Create ROOT.
			names = namemap;
			ids[0] = AddRoot(towxstring(namemap[0] + ((!nosels && current == 0) ? " <selected>" : "")));
			build_tree(0, ids[0], childmap, namemap);
			for(auto i : expanded)
				if(ids.count(i))
					Expand(ids[i]);
			for(auto i : ids) {
				if(i.first == cursel.id) {
					SelectItem(i.second);
				}
			}
		}
		std::string get_name(uint64_t id)
		{
			if(names.count(id))
				return names[id];
			return "";
		}
	private:
		void build_tree(uint64_t id, wxTreeItemId parent, std::map<uint64_t, std::set<uint64_t>>& childmap,
			std::map<uint64_t, std::string>& namemap)
		{
			CHECK_UI_THREAD;
			if(!childmap.count(id) || childmap[id].empty())
				return;
			for(auto i : childmap[id]) {
				ids[i] = AppendItem(ids[id], towxstring(namemap[i] + ((!nosels && current == i) ?
					" <selected>" : "")));
				build_tree(i, ids[i], childmap, namemap);
			}
		}
		emulator_instance& inst;
		uint64_t current;
		std::map<uint64_t, std::string> names;
		std::map<uint64_t, wxTreeItemId> ids;
		std::set<uint64_t> with_children;
		struct dispatch::target<> branchchange;
		bool nosels;
	};

	class branch_select : public wxDialog
	{
	public:
		branch_select(wxWindow* parent, emulator_instance& _inst)
			: wxDialog(parent, wxID_ANY, towxstring("lsnes: Select new parent branch"))
		{
			CHECK_UI_THREAD;
			Centre();
			wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
			SetSizer(top_s);
			Center();

			top_s->Add(branches = new branches_tree(this, _inst, wxID_ANY, true), 1, wxGROW);
			branches->Connect(wxEVT_COMMAND_TREE_SEL_CHANGED,
				wxCommandEventHandler(branch_select::on_change), NULL, this);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->AddStretchSpacer();
			pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
			pbutton_s->Add(cancelbutton = new wxButton(this, wxID_OK, wxT("Cancel")), 0, wxGROW);
			okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_select::on_ok), NULL, this);
			cancelbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_select::on_cancel), NULL, this);
			top_s->Add(pbutton_s, 0, wxGROW);

			top_s->SetSizeHints(this);
			Fit();

			wxCommandEvent e;
			on_change(e);
		}
		void on_ok(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			EndModal(wxID_OK);
		}
		void on_cancel(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			EndModal(wxID_CANCEL);
		}
		uint64_t get_selection()
		{
			return branches->get_selection().id;
		}
		void on_change(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			okbutton->Enable(get_selection() != 0xFFFFFFFFFFFFFFFFULL);
		}
	private:
		wxButton* okbutton;
		wxButton* cancelbutton;
		branches_tree* branches;
	};

	class branch_config : public wxDialog
	{
	public:
		branch_config(wxWindow* parent, emulator_instance& _inst)
			: wxDialog(parent, wxID_ANY, towxstring("lsnes: Edit slot branches")), inst(_inst)
		{
			CHECK_UI_THREAD;
			Centre();
			wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
			SetSizer(top_s);
			Center();

			top_s->Add(branches = new branches_tree(this, inst, wxID_ANY, false), 1, wxGROW);
			branches->Connect(wxEVT_COMMAND_TREE_SEL_CHANGED,
				wxCommandEventHandler(branch_config::on_change), NULL, this);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->Add(createbutton = new wxButton(this, wxID_OK, wxT("Create")), 0, wxGROW);
			pbutton_s->Add(selectbutton = new wxButton(this, wxID_OK, wxT("Select")), 0, wxGROW);
			pbutton_s->Add(renamebutton = new wxButton(this, wxID_OK, wxT("Rename")), 0, wxGROW);
			pbutton_s->Add(reparentbutton = new wxButton(this, wxID_OK, wxT("Reparent")), 0, wxGROW);
			pbutton_s->Add(deletebutton = new wxButton(this, wxID_OK, wxT("Delete")), 0, wxGROW);
			pbutton_s->AddStretchSpacer();
			pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("Close")), 0, wxGROW);
			createbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_config::on_create), NULL, this);
			selectbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_config::on_select), NULL, this);
			renamebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_config::on_rename), NULL, this);
			reparentbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_config::on_reparent), NULL, this);
			deletebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_config::on_delete), NULL, this);
			okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(branch_config::on_close), NULL, this);
			top_s->Add(pbutton_s, 0, wxGROW);

			top_s->SetSizeHints(this);
			Fit();
			wxCommandEvent e;
			on_change(e);
		}
		void on_create(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			uint64_t id = get_selected_id();
			if(id == 0xFFFFFFFFFFFFFFFFULL) return;
			std::string newname;
			try {
				newname = pick_text(this, "Enter new branch name", "Enter name for new branch:",
					newname, false);
			} catch(canceled_exception& e) {
				return;
			}
			UI_create_branch(inst, id, newname, [this](std::exception& e) {
				show_exception_any(this, "Error creating branch", "Can't create branch", e);
			});
		}
		void on_select(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			uint64_t id = get_selected_id();
			if(id == 0xFFFFFFFFFFFFFFFFULL) return;
			UI_switch_branch(inst, id, [this](std::exception& e) {
				show_exception_any(this, "Error setting branch", "Can't set branch", e);
			});
		}
		void on_rename(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			uint64_t id = get_selected_id();
			if(id == 0xFFFFFFFFFFFFFFFFULL) return;
			std::string newname = branches->get_name(id);
			try {
				newname = pick_text(this, "Enter new branch name", "Rename this branch to:",
					newname, false);
			} catch(canceled_exception& e) {
				return;
			}
			UI_rename_branch(inst, id, newname, [this](std::exception& e) {
				show_exception_any(this, "Error renaming branch", "Can't rename branch", e);
			});
		}
		void on_reparent(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			uint64_t id = get_selected_id();
			if(id == 0xFFFFFFFFFFFFFFFFULL) return;
			uint64_t pid;
			branch_select* bsel = new branch_select(this, inst);
			int r = bsel->ShowModal();
			if(r != wxID_OK) {
				bsel->Destroy();
				return;
			}
			pid = bsel->get_selection();
			if(pid == 0xFFFFFFFFFFFFFFFFULL) return;
			bsel->Destroy();
			UI_reparent_branch(inst, id, pid, [this](std::exception& e) {
				show_exception_any(this, "Error reparenting branch", "Can't reparent branch", e);
			});
		}
		void on_delete(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			uint64_t id = get_selected_id();
			if(id == 0xFFFFFFFFFFFFFFFFULL) return;
			UI_delete_branch(inst, id, [this](std::exception& e) {
				show_exception_any(this, "Error deleting branch", "Can't delete branch", e);
			});
		}
		void on_close(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			EndModal(wxID_OK);
		}
		void on_change(wxCommandEvent& e)
		{
			set_enabled(branches->get_selection());
		}
	private:
		uint64_t get_selected_id()
		{
			return branches->get_selection().id;
		}
		void set_enabled(branches_tree::selection id)
		{
			CHECK_UI_THREAD;
			createbutton->Enable(id.item.IsOk());
			selectbutton->Enable(id.item.IsOk());
			renamebutton->Enable(id.item.IsOk() && !id.isroot);
			reparentbutton->Enable(id.item.IsOk() && !id.isroot);
			deletebutton->Enable(id.item.IsOk() && !id.isroot && !id.haschildren && !id.iscurrent);
		}
		wxButton* createbutton;
		wxButton* selectbutton;
		wxButton* renamebutton;
		wxButton* reparentbutton;
		wxButton* deletebutton;
		wxButton* okbutton;
		branches_tree* branches;
		emulator_instance& inst;
	};

	void build_menus(wxMenu* root, uint64_t id, std::list<branches_menu::miteminfo>& otheritems,
		std::list<wxMenu*>& menus, std::map<uint64_t, std::string>& namemap,
		std::map<uint64_t, std::set<uint64_t>>& childmap, std::map<int, uint64_t>& branch_ids, int& nextid,
		uint64_t curbranch)
	{
		CHECK_UI_THREAD;
		auto& children = childmap[id];
		int mid = nextid++;
		otheritems.push_back(branches_menu::miteminfo(root->AppendCheckItem(mid, towxstring(namemap[id])),
			false, root));
		branch_ids[mid] = id;
		root->FindItem(mid)->Check(id == curbranch);
		if(!children.empty())
			otheritems.push_back(branches_menu::miteminfo(root->AppendSeparator(), false, root));
		for(auto i : children) {
			bool has_children = !childmap[i].empty();
			if(!has_children) {
				//No children, just put the item there.
				int mid2 = nextid++;
				otheritems.push_back(branches_menu::miteminfo(root->AppendCheckItem(mid2,
					towxstring(namemap[i])), false, root));
				branch_ids[mid2] = i;
				root->FindItem(mid2)->Check(i == curbranch);
			} else {
				//Has children. Make a menu.
				wxMenu* m = new wxMenu();
				otheritems.push_back(branches_menu::miteminfo(root->AppendSubMenu(m,
					towxstring(namemap[i])), true, root));
				menus.push_back(m);
				build_menus(m, i, otheritems, menus, namemap, childmap, branch_ids, nextid,
					curbranch);
			}
		}
	}
}

branches_menu::branches_menu(wxWindow* win, emulator_instance& _inst, int wxid_low, int wxid_high)
	: inst(_inst)
{
	CHECK_UI_THREAD;
	pwin = win;
	wxid_range_low = wxid_low;
	wxid_range_high = wxid_high;
	win->Connect(wxid_low, wxid_high, wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(branches_menu::on_select), NULL, this);
	branchchange.set(inst.dispatch->branch_change, [this]() { runuifun([this]() { this->update(); }); });
}

branches_menu::~branches_menu()
{
}

void branches_menu::on_select(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	int id = e.GetId();
	if(id < wxid_range_low || id > wxid_range_high) return;
	if(id == wxid_range_low) {
		//Configure.
		branch_config* bcfg = new branch_config(pwin, inst);
		bcfg->ShowModal();
		bcfg->Destroy();
		return;
	}
	if(!branch_ids.count(id)) return;
	uint64_t bid = branch_ids[id];
	std::string err;
	UI_switch_branch(inst, bid, [this](std::exception& e) {
		show_exception_any(this->pwin, "Error changing branch", "Can't change branch", e);
	});
}

void branches_menu::update()
{
	CHECK_UI_THREAD;
	std::map<uint64_t, std::string> namemap;
	std::map<uint64_t, std::set<uint64_t>> childmap;
	uint64_t cur;
	UI_get_branch_map(inst, cur, namemap, childmap);
	//First destroy everything that isn't a menu.
	for(auto i : otheritems)
		i.parent->Delete(i.item);
	//Then kill all menus.
	for(auto i : menus)
		delete i;
	otheritems.clear();
	menus.clear();
	branch_ids.clear();
	if(namemap.empty()) {
		if(disabler_fn) disabler_fn(false);
		return;
	}
	//Okay, cleared. Rebuild things.
	otheritems.push_back(miteminfo(Append(wxid_range_low, towxstring("Edit branches")), false, this));
	otheritems.push_back(miteminfo(AppendSeparator(), false, this));
	int ass_id = wxid_range_low + 1;
	build_menus(this, 0, otheritems, menus, namemap, childmap, branch_ids, ass_id, cur);
	if(disabler_fn) disabler_fn(true);
}

bool branches_menu::any_enabled()
{
	return UI_in_project_context(inst);
}
