#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"
#include "platform/wxwidgets/window-romload.hpp"
#include "core/command.hpp"
#include "core/instance.hpp"
#include "core/mainloop.hpp"
#include "core/messages.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/romtype.hpp"
#include "library/zip.hpp"

namespace
{
	bool can_load_singlefile(core_type* t)
	{
		unsigned icnt = t->get_image_count();
		unsigned pmand = 0, tmand = 0;
		if(!icnt) return false;
		if(icnt > 0) pmand |= t->get_image_info(0).mandatory;
		if(t->get_biosname() != "" && icnt > 1) pmand |= t->get_image_info(1).mandatory;
		for(unsigned i = 0; i < t->get_image_count(); i++)
			tmand |= t->get_image_info(i).mandatory;
		return pmand == tmand;
	}

	std::string implode_set(const std::set<std::string>& s)
	{
		std::string a;
		for(auto i : s) {
			if(a != "") a += ";";
			a += ("*." + i);
		}
		return a;
	}

	int resolve_core(std::map<unsigned, core_type*> coreid, const std::string& filename, int findex)
	{
		CHECK_UI_THREAD;
		if(coreid.count(findex))
			return findex;	//Already resolved.
		if(rom_image::is_gamepak(filename))
			return 0; //Gamepaks don't resolve.

		//Get the extension.
		regex_results r = regex(".*\\.([^.]+)", filename);
		if(!r)
			return 0; //WTF is this? Leave unresolved.
		std::string extension = r[1];

		std::map<core_type*, unsigned> candidates;
		for(auto i : coreid) {
			if(!can_load_singlefile(i.second))
				continue;
			if(i.second->is_hidden())
				continue;
			if(i.second->isnull())
				continue;
			std::set<std::string> exts;
			unsigned base = (i.second->get_biosname() != "" && i.second->get_image_count() > 1) ? 1 : 0;
			for(auto j : i.second->get_image_info(base).extensions)
				if(j == extension) {
					//This is a candidate.
					candidates[i.second] = i.first;
				}
		}

		if(candidates.empty())
			return 0;	//Err. Leave unresolved.
		if(candidates.size() == 1)
			return candidates.begin()->second;	//Only one candidate.

		//Okay, we have multiple candidates. Prompt among them.
		std::vector<std::string> choices;
		std::vector<unsigned> indexes;
		for(auto i : candidates) {
			choices.push_back(i.first->get_core_identifier() + " [" + i.first->get_hname() + "]");
			indexes.push_back(i.second);
		}
		std::string coretext;
		coretext = pick_among(NULL, "Choose core", "Choose core to load the ROM", choices, 0);
		for(size_t i = 0; i < choices.size(); i++)
			if(choices[i] == coretext)
				return indexes[i];
		//Err?
		return 0;
	}

	void do_load_rom_image_single(wxwin_mainwindow* parent, emulator_instance& inst)
	{
		CHECK_UI_THREAD;
		std::map<std::string, core_type*> cores;
		std::map<unsigned, core_type*> coreid;
		std::string filter;
		unsigned corecount = 0;
		std::set<std::string> all_filetypes;
		all_filetypes.insert("lsgp");
		for(auto i : core_type::get_core_types()) {
			if(!can_load_singlefile(i))
				continue;
			unsigned base = (i->get_biosname() != "" && i->get_image_count() > 1) ? 1 : 0;
			for(auto j : i->get_image_info(base).extensions)
				all_filetypes.insert(j);
			cores[i->get_hname() + " [" + i->get_core_identifier() + "]"] = i;
		}
		filter += "Autodetect|" + implode_set(all_filetypes);
		for(auto i : cores) {
			if(!can_load_singlefile(i.second))
				continue;
			if(i.second->is_hidden())
				continue;
			if(i.second->isnull())
				continue;
			std::set<std::string> exts;
			unsigned base = (i.second->get_biosname() != "" && i.second->get_image_count() > 1) ? 1 : 0;
			for(auto j : i.second->get_image_info(base).extensions)
				exts.insert(j);
			filter += "|" + i.first + "|" + implode_set(exts);
			coreid[++corecount] = i.second;
		}
		filter += "|All files|*";
		std::string directory = SET_rompath(*inst.settings);
		wxFileDialog* d = new wxFileDialog(parent, towxstring("Choose ROM to load"), towxstring(directory),
			wxT(""), towxstring(filter), wxFD_OPEN);
		if(d->ShowModal() == wxID_CANCEL) {
			delete d;
			return;
		}
		romload_request req;
		recentfiles::multirom mr;
		std::string filename = tostdstring(d->GetPath());
		int findex = d->GetFilterIndex();
		try {
			findex = resolve_core(coreid, filename, findex);
		} catch(canceled_exception& e) {
			return;
		}
		if(!coreid.count(findex)) {
			//Autodetect.
			mr.packfile = req.packfile = filename;
		} else {
			mr.core = req.core = coreid[findex]->get_core_identifier();
			mr.system = req.system = coreid[findex]->get_iname();
			mr.singlefile = req.singlefile = filename;
		}
		parent->recent_roms->add(mr);
		inst.iqueue->run_async([req]() {
			CORE().command->invoke("unpause-emulator");
			load_new_rom(req);
		}, [](std::exception& e) {});
	}

	class multirom_dialog : public wxDialog
	{
	public:
		void on_wclose(wxCloseEvent& e)
		{
			CHECK_UI_THREAD;
			EndModal(wxID_CANCEL);
		}
		multirom_dialog(wxWindow* parent, emulator_instance& _inst, std::string rtype, core_type& _t)
			: wxDialog(parent, wxID_ANY, towxstring("lsnes: Load " + rtype + " ROM")), inst(_inst), t(_t)
		{
			CHECK_UI_THREAD;
			Centre();
			wxSizer* vsizer = new wxBoxSizer(wxVERTICAL);
			SetSizer(vsizer);
			Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(multirom_dialog::on_wclose), NULL, this);

			wxSizer* hsizer2 = new wxBoxSizer(wxHORIZONTAL);
			hsizer2->Add(new wxStaticText(this, wxID_ANY, wxT("Region: ")), 0, wxGROW);
			std::vector<wxString> regions_list;
			core_region& prefr = t.get_preferred_region();
			unsigned regindex = 0;
			for(auto i : t.get_regions()) {
				if(i == &prefr) regindex = regions_list.size();
				regions_list.push_back(towxstring(i->get_hname()));
				regions_known.push_back(i);
			}
			regions = new wxComboBox(this, wxID_ANY, regions_list[regindex], wxDefaultPosition,
				wxDefaultSize, regions_list.size(), &regions_list[0], wxCB_READONLY);
			hsizer2->Add(regions, 0, wxGROW);
			vsizer->Add(hsizer2, 0, wxGROW);

			wxSizer* rarray = new wxFlexGridSizer(2 * t.get_image_count(), 3, 0, 0);
			for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
				filenames[i] = NULL;
				fileselect[i] = NULL;
			}
			for(unsigned i = 0; i < t.get_image_count(); i++) {
				core_romimage_info iinfo = t.get_image_info(i);
				rarray->Add(new wxStaticText(this, wxID_ANY, towxstring(iinfo.hname)), 0, wxGROW);
				rarray->Add(filenames[i] = new wxTextCtrl(this, wxID_HIGHEST + 100 + i, wxT(""),
					wxDefaultPosition, wxSize(400, -1)), 1, wxGROW);
				rarray->Add(fileselect[i] = new wxButton(this, wxID_HIGHEST + 200 + i,
					towxstring("...")), 0, wxGROW);
				rarray->Add(new wxStaticText(this, wxID_ANY, towxstring("")), 0, wxGROW);
				rarray->Add(hashes[i] = new wxStaticText(this, wxID_ANY, wxT("Not found")), 1,
					wxGROW);
				rarray->Add(new wxStaticText(this, wxID_ANY, towxstring("")), 0, wxGROW);
				hash_ready[i] = true;
				filenames[i]->Connect(wxEVT_COMMAND_TEXT_UPDATED,
					wxCommandEventHandler(multirom_dialog::do_command_event), NULL, this);
				fileselect[i]->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
					wxCommandEventHandler(multirom_dialog::do_command_event), NULL, this);
			}
			vsizer->Add(rarray, 1, wxGROW);

			wxSizer* buttonbar = new wxBoxSizer(wxHORIZONTAL);
			buttonbar->Add(okb = new wxButton(this, wxID_HIGHEST + 1, wxT("Load")), 0, wxGROW);
			buttonbar->AddStretchSpacer();
			buttonbar->Add(cancelb = new wxButton(this, wxID_HIGHEST + 2, wxT("Cancel")), 0, wxGROW);
			vsizer->Add(buttonbar, 0, wxGROW);
			okb->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(multirom_dialog::do_command_event), NULL, this);
			okb->Disable();
			cancelb->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(multirom_dialog::do_command_event), NULL, this);
			timer = new update_timer(this);
			timer->Start(100);
			vsizer->SetSizeHints(this);
			Fit();
		}
		~multirom_dialog()
		{
			CHECK_UI_THREAD;
			if(timer) {
				timer->Stop();
				delete timer;
			}
		}
		void timer_update_hashes()
		{
			CHECK_UI_THREAD;
			for(auto i = 0; i < ROM_SLOT_COUNT && filenames[i]; i++) {
				if(hash_ready[i])
					continue;
				if(!hashfutures[i].ready())
					continue;
				try {
					hashes[i]->SetLabel(towxstring("Hash: " + hashfutures[i].read()));
				} catch(std::runtime_error& e) {
					hashes[i]->SetLabel(towxstring("Hash: Error"));
				}
				hash_ready[i] = true;
			}
		}
		void do_command_event(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			int id = e.GetId();
			if(id == wxID_HIGHEST + 1) {
				if(!check_requirements()) {
					show_message_ok(this, "Needed ROM missing", "At least one required ROM "
						"is missing!", wxICON_EXCLAMATION);
					return;
				}
				EndModal(wxID_OK);
			} else if(id == wxID_HIGHEST + 2) {
				EndModal(wxID_CANCEL);
			} else if(id >= wxID_HIGHEST + 100 && id <= wxID_HIGHEST + 199) {
				filename_updated(id - (wxID_HIGHEST + 100));
			} else if(id >= wxID_HIGHEST + 200 && id <= wxID_HIGHEST + 299) {
				do_fileselect(id - (wxID_HIGHEST + 200));
			}
		}
		std::string getfilename(unsigned i)
		{
			CHECK_UI_THREAD;
			if(i >= ROM_SLOT_COUNT || !filenames[i])
				return "";
			return tostdstring(filenames[i]->GetValue());
		}
		std::string getregion()
		{
			CHECK_UI_THREAD;
			int i = regions->GetSelection();
			if(i == wxNOT_FOUND)
				return t.get_preferred_region().get_iname();
			return regions_known[i]->get_iname();
		}
	private:
		bool check_requirements()
		{
			CHECK_UI_THREAD;
			unsigned pm = 0, tm = 0;
			for(unsigned i = 0; i < t.get_image_count(); i++) {
				core_romimage_info iinfo = t.get_image_info(i);
				tm |= iinfo.mandatory;
				if(filenames[i]->GetValue().Length() != 0)
					pm |= iinfo.mandatory;
			}
			return pm == tm;
		}
		void do_fileselect(unsigned i)
		{
			CHECK_UI_THREAD;
			if(i >= ROM_SLOT_COUNT || !fileselect[i])
				return;
			std::string filter;
			std::set<std::string> exts;
			for(auto j : t.get_image_info(i).extensions)
				exts.insert(j);
			filter = "Known file types|" + implode_set(exts) + "|All files|*";
			auto _directory = &SET_firmwarepath;
			if(t.get_biosname() != "" && i == 0)
				_directory = &SET_firmwarepath;
			else
				_directory = &SET_rompath;
			std::string directory = (*_directory)(*inst.settings);
			core_romimage_info iinfo = t.get_image_info(i);
			wxFileDialog* d = new wxFileDialog(this, towxstring("Load " + iinfo.hname),
				towxstring(directory), wxT(""), towxstring(filter), wxFD_OPEN);
			if(d->ShowModal() == wxID_CANCEL) {
				delete d;
				return;
			}
			filenames[i]->SetValue(d->GetPath());
			delete d;
		}
		void filename_updated(unsigned i)
		{
			CHECK_UI_THREAD;
			if(i >= t.get_image_count() || !filenames[i])
				return;
			uint64_t header = t.get_image_info(i).headersize;

			std::string filename = tostdstring(filenames[i]->GetValue());
			if(!zip::file_exists(filename)) {
				hashfutures[i] = fileimage::hashval();
				hash_ready[i] = true;
				hashes[i]->SetLabel(towxstring("Not found"));
				return;
			}
			//TODO: Handle files inside ZIP files.
			hashfutures[i] = lsnes_image_hasher(filename, fileimage::std_headersize_fn(header));
			if((hash_ready[i] = hashfutures[i].ready()))
				try {
					hashes[i]->SetLabel(towxstring("Hash: " + hashfutures[i].read()));
				} catch(std::runtime_error& e) {
					hashes[i]->SetLabel(towxstring("Hash: Error"));
				}
			else
				hashes[i]->SetLabel(towxstring("Hash: Calculating..."));
			okb->Enable(check_requirements());
		}
		struct update_timer : public wxTimer
		{
		public:
			update_timer(multirom_dialog* p)
			{
				w = p;
			}
			void Notify()
			{
				w->timer_update_hashes();
			}
		private:
			multirom_dialog* w;
		};
		emulator_instance& inst;
		wxComboBox* regions;
		wxTextCtrl* filenames[ROM_SLOT_COUNT];
		wxButton* fileselect[ROM_SLOT_COUNT];
		wxStaticText* hashes[ROM_SLOT_COUNT];
		bool hash_ready[ROM_SLOT_COUNT];
		fileimage::hashval hashfutures[ROM_SLOT_COUNT];
		wxButton* okb;
		wxButton* cancelb;
		update_timer* timer;
		core_type& t;
		std::vector<core_region*> regions_known;
	};

	void do_load_rom_image_multiple(wxwin_mainwindow* parent, emulator_instance& inst, core_type& t)
	{
		CHECK_UI_THREAD;
		multirom_dialog* d = new multirom_dialog(parent, inst, t.get_hname(), t);
		if(d->ShowModal() == wxID_CANCEL) {
			delete d;
			return;
		}
		std::string files[ROM_SLOT_COUNT];
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++)
			files[i] = d->getfilename(i);
		std::string region = d->getregion();
		delete d;
		romload_request req;
		recentfiles::multirom mr;
		mr.core = req.core = t.get_core_identifier();
		mr.system = req.system = t.get_iname();
		mr.region = req.region = region;
		for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
			if(files[i] != "") {
				mr.files.resize(i + 1);
				mr.files[i] = files[i];
			}
			req.files[i] = files[i];
		}
		parent->recent_roms->add(mr);
		inst.iqueue->run([req]() {
			CORE().command->invoke("unpause-emulator");
			load_new_rom(req);
		});
		return;
	}
}

void wxwin_mainwindow::request_rom(rom_request& req)
{
	CHECK_UI_THREAD;
	std::vector<std::string> choices;
	for(auto i : req.cores)
		choices.push_back(i->get_core_identifier());
	std::string coretext;
	try  {
		if(choices.size() > 1 && !req.core_guessed)
			coretext = pick_among(this, "Choose core", "Choose core to load the ROM", choices,
				req.selected);
		else
			coretext = choices[req.selected];
	} catch(canceled_exception& e) {
		return;
	}
	for(size_t i = 0; i < req.cores.size(); i++)
		if(coretext == req.cores[i]->get_core_identifier())
			req.selected = i;
	core_type& type = *req.cores[req.selected];

	bool has_bios = (type.get_biosname() != "");
	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		if(!req.has_slot[i])
			continue;
		if(req.guessed[i])
			continue; //Leave these alone.
		if(i >= type.get_image_count()) {
			messages << "wxwin_mainwindow::request_rom: Present image index (" << i
				<< ") out of range!" << std::endl;
			continue;		//Shouldn't happen.
		}
		core_romimage_info iinfo = type.get_image_info(i);
		auto _directory = &SET_rompath;
		if(i == 0 && has_bios)
			_directory = &SET_firmwarepath;
		else
			_directory = &SET_rompath;
		std::string directory = (*_directory)(*inst.settings);
		std::string _title = "Select " + iinfo.hname;
		std::string filespec = "Known ROMs|";
		std::string exts = "";
		std::string defaultname = "";
		for(auto j : iinfo.extensions) {
			exts = exts + ";*." + j;
			if(zip::file_exists(directory + "/" + req.filename[i] + "." + j))
				defaultname = req.filename[i] + "." + j;
		}
		if(exts != "") exts = exts.substr(1);
		filespec = "Known ROMs (" + exts + ")|" + exts + "|All files|*";
		wxFileDialog* d;
		std::string hash;
		uint64_t header = type.get_image_info(i).headersize;
again:
		d = new wxFileDialog(this, towxstring(_title), towxstring(directory), wxT(""),
			towxstring(filespec), wxFD_OPEN);
		if(defaultname != "") d->SetFilename(towxstring(defaultname));
		if(d->ShowModal() == wxID_CANCEL) {
			delete d;
			return;
		}
		req.filename[i] = tostdstring(d->GetPath());
		delete d;
		//Check the hash.
		if(!zip::file_exists(req.filename[i])) {
			show_message_ok(this, "File not found", "Can't find '" + req.filename[i] + "'",
				wxICON_EXCLAMATION);
			goto again;
		}
		try {
			auto future = lsnes_image_hasher(req.filename[i], fileimage::std_headersize_fn(header));
			//Dirty method to run the event loop until hashing finishes.
			while(!future.ready()) {
				wxSafeYield();
				usleep(50000);
			}
			std::string hash = future.read();
			if(hash != req.hash[i]) {
				//Hash mismatches.
				wxMessageDialog* d3 = new wxMessageDialog(this, towxstring("The ROM checksum does "
					"not match movie\n\nProceed anyway?"), towxstring("Checksum error"),
					wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION);
				int r = d3->ShowModal();
				d3->Destroy();
				if(r == wxID_NO) goto again;
			}
		} catch(...) {
			wxMessageDialog* d3 = new wxMessageDialog(this, towxstring("Can't read checksum for "
				"ROM\n\nProceed anyway?"), towxstring("Checksum error"), wxYES_NO |
				wxYES_DEFAULT | wxICON_EXCLAMATION);
			int r = d3->ShowModal();
			d3->Destroy();
			if(r == wxID_NO) goto again;
		}
	}
	req.canceled = false;
}

void wxwin_mainwindow::do_load_rom_image(core_type* t)
{
	if(!t) {
		return do_load_rom_image_single(this, inst);
	} else {
		return do_load_rom_image_multiple(this, inst, *t);
	}
}
