/***************************************************************************
 *   Copyright (C) 2007 by Sindre Aamås                                    *
 *   sinamas@users.sourceforge.net                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License version 2 for more details.                *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   version 2 along with this program; if not, write to the               *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "gambatte.h"
#include "cpu.h"
#include "initstate.h"
#include "savestate.h"
#include "state_osd_elements.h"
#include "statesaver.h"
#include <cstring>
#include <sstream>

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

static std::string const itos(int i) {
	std::stringstream ss;
	ss << i;
	return ss.str();
}

static std::string const statePath(std::string const &basePath, int stateNo) {
	return basePath + "_" + itos(stateNo) + ".gqs";
}

namespace
{
	time_t default_walltime()
	{
		return time(0);
	}
}

namespace gambatte {

struct GB::Priv {
	CPU cpu;
	int stateNo;
	unsigned loadflags;

	Priv(time_t (**_getCurrentTime)()) : stateNo(1), loadflags(0), cpu(_getCurrentTime) {}
};

GB::GB() : p_(new Priv(&walltime)), walltime(default_walltime) {}

GB::~GB() {
	if (p_->cpu.loaded())
		p_->cpu.saveSavedata();

	delete p_;
}

signed GB::runFor(gambatte::uint_least32_t *const videoBuf, std::ptrdiff_t const pitch,
                gambatte::uint_least32_t *const soundBuf, unsigned &samples) {
	if (!p_->cpu.loaded()) {
		samples = 0;
		return -1;
	}

	p_->cpu.setVideoBuffer(videoBuf, pitch);
	p_->cpu.setSoundBuffer(soundBuf);

	signed const cyclesSinceBlit = p_->cpu.runFor(samples * 2);
	samples = p_->cpu.fillSoundBuffer();
	return cyclesSinceBlit >= 0
	     ? static_cast<signed>(samples) - (cyclesSinceBlit >> 1)
	     : cyclesSinceBlit;
}

void GB::reset() {
	if (p_->cpu.loaded()) {
		p_->cpu.saveSavedata();

		SaveState state;
		p_->cpu.setStatePtrs(state);
		setInitState(state, p_->cpu.isCgb(), p_->loadflags & GBA_CGB, walltime());
		p_->cpu.loadState(state);
		p_->cpu.loadSavedata();
	}
}

void GB::setInputGetter(InputGetter *getInput) {
	p_->cpu.setInputGetter(getInput);
}

void GB::setSaveDir(std::string const &sdir) {
	p_->cpu.setSaveDir(sdir);
}

void GB::preload_common()
{
	if (p_->cpu.loaded())
		p_->cpu.saveSavedata();
}

void GB::postload_common(const unsigned flags)
{
	SaveState state;
	p_->cpu.setStatePtrs(state);
	setInitState(state, p_->cpu.isCgb(), flags & GBA_CGB, walltime());
	p_->cpu.loadState(state);
	p_->cpu.loadSavedata();

	p_->stateNo = 1;
	p_->cpu.setOsdElement(transfer_ptr<OsdElement>());
}

LoadRes GB::load(std::string const &romfile, unsigned const flags) {
	preload_common();

	LoadRes const loadres = p_->cpu.load(romfile,
	                                     flags & FORCE_DMG,
	                                     flags & MULTICART_COMPAT);

	if (loadres == LOADRES_OK)
		postload_common(flags);

	return loadres;
}

LoadRes GB::load(const unsigned char* image, size_t isize, unsigned flags) {
	preload_common();

	LoadRes const loadres = p_->cpu.load(image, isize, flags & FORCE_DMG, flags & MULTICART_COMPAT);

	if (loadres == LOADRES_OK)
		postload_common(flags);

	return loadres;
}

bool GB::isCgb() const {
	return p_->cpu.isCgb();
}

bool GB::isLoaded() const {
	return p_->cpu.loaded();
}

void GB::saveSavedata() {
	if (p_->cpu.loaded())
		p_->cpu.saveSavedata();
}

void GB::setDmgPaletteColor(int palNum, int colorNum, uint_least32_t rgb32) {
	p_->cpu.setDmgPaletteColor(palNum, colorNum, rgb32);
}

bool GB::loadState(std::string const &filepath) {
	if (p_->cpu.loaded()) {
		p_->cpu.saveSavedata();

		SaveState state;
		p_->cpu.setStatePtrs(state);

		if (StateSaver::loadState(state, filepath)) {
			p_->cpu.loadState(state);
			return true;
		}
	}

	return false;
}

bool GB::saveState(gambatte::uint_least32_t const *videoBuf, std::ptrdiff_t pitch) {
	if (saveState(videoBuf, pitch, statePath(p_->cpu.saveBasePath(), p_->stateNo))) {
		p_->cpu.setOsdElement(newStateSavedOsdElement(p_->stateNo));
		return true;
	}

	return false;
}

bool GB::loadState() {
	if (loadState(statePath(p_->cpu.saveBasePath(), p_->stateNo))) {
		p_->cpu.setOsdElement(newStateLoadedOsdElement(p_->stateNo));
		return true;
	}

	return false;
}

bool GB::saveState(gambatte::uint_least32_t const *videoBuf, std::ptrdiff_t pitch,
                   std::string const &filepath) {
	if (p_->cpu.loaded()) {
		SaveState state;
		p_->cpu.setStatePtrs(state);
		p_->cpu.saveState(state);
		return StateSaver::saveState(state, videoBuf, pitch, filepath);
	}

	return false;
}

void GB::saveState(std::vector<char>& data, const std::vector<char>& cmpdata) {
	if (p_->cpu.loaded()) {
		loadsave_save l(cmpdata);
		p_->cpu.loadOrSave(l);
		data = l.get();
	}
}

void GB::saveState(std::vector<char>& data) {
	if (p_->cpu.loaded()) {
		loadsave_save l;
		p_->cpu.loadOrSave(l);
		data = l.get();
	}
}

void GB::loadState(const std::vector<char>& data) {
	if (p_->cpu.loaded()) {
		loadsave_load l(data);
		p_->cpu.loadOrSave(l);
	}
}

void GB::selectState(int n) {
	n -= (n / 10) * 10;
	p_->stateNo = n < 0 ? n + 10 : n;

	if (p_->cpu.loaded()) {
		std::string const &path = statePath(p_->cpu.saveBasePath(), p_->stateNo);
		p_->cpu.setOsdElement(newSaveStateOsdElement(path, p_->stateNo));
	}
}

int GB::currentState() const { return p_->stateNo; }

std::string const GB::romTitle() const {
	if (p_->cpu.loaded()) {
		char title[0x11];
		std::memcpy(title, p_->cpu.romTitle(), 0x10);
		title[title[0xF] & 0x80 ? 0xF : 0x10] = '\0';
		return std::string(title);
	}

	return std::string();
}

PakInfo const GB::pakInfo() const { return p_->cpu.pakInfo(p_->loadflags & MULTICART_COMPAT); }

void GB::setGameGenie(std::string const &codes) {
	p_->cpu.setGameGenie(codes);
}

void GB::setGameShark(std::string const &codes) {
	p_->cpu.setGameShark(codes);
}

void GB::setRtcBase(time_t time) {
	p_->cpu.setRtcBase(time);
}

time_t GB::getRtcBase() {
	return p_->cpu.getRtcBase();
}

std::pair<unsigned char*, size_t> GB::getWorkRam() {
	return p_->cpu.getWorkRam();
}

std::pair<unsigned char*, size_t> GB::getSaveRam() {
	return p_->cpu.getSaveRam();
}

std::pair<unsigned char*, size_t> GB::getIoRam() {
	return p_->cpu.getIoRam();
}

std::pair<unsigned char*, size_t> GB::getVideoRam() {
	return p_->cpu.getVideoRam();
}

void GB::set_walltime_fn(time_t (*_walltime)())
{
	walltime = _walltime;
}

std::string GB::version()
{
	return "r537";
}

uint32_t GB::get_cpureg(enum cpu_register _reg)
{
	switch(_reg) {
		case REG_CYCLECOUNTER:	return *p_->cpu.cyclecountptr;
		case REG_PC:		return *p_->cpu.pcptr;
		case REG_SP:		return p_->cpu.sp;
		case REG_HF1:		return p_->cpu.hf1;
		case REG_HF2:		return p_->cpu.hf2;
		case REG_ZF:		return p_->cpu.zf;
		case REG_CF:		return p_->cpu.cf;
		case REG_A:		return *p_->cpu.aptr;
		case REG_B:		return p_->cpu.b;
		case REG_C:		return p_->cpu.c;
		case REG_D:		return p_->cpu.d;
		case REG_E:		return p_->cpu.e;
		case REG_F:
			return ((p_->cpu.hf2 & 0x600 | (p_->cpu.cf & 0x100)) >> 4)
				| (p_->cpu.zf & 0xFF ? 0 : 0x80);
		case REG_H:		return p_->cpu.h;
		case REG_L:		return p_->cpu.l;
		default:		return 0;
	}
}

void GB::set_cpureg(enum cpu_register _reg, uint32_t val)
{
	switch(_reg) {
		case REG_PC:		*p_->cpu.pcptr = val; break;
		case REG_SP:		p_->cpu.sp = val; break;
		case REG_HF1:		p_->cpu.hf1 = val; break;
		case REG_HF2:		p_->cpu.hf2 = val; break;
		case REG_ZF:		p_->cpu.zf = val; break;
		case REG_CF:		p_->cpu.cf = val; break;
		case REG_A:		*p_->cpu.aptr = val; break;
		case REG_B:		p_->cpu.b = val; break;
		case REG_C:		p_->cpu.c = val; break;
		case REG_D:		p_->cpu.d = val; break;
		case REG_E:		p_->cpu.e = val; break;
		case REG_F:
			p_->cpu.hf2 = (val << 4) & 0x600;
			p_->cpu.cf = (val << 4) & 0x100;
			p_->cpu.zf = val & 0x80;
			break;
		case REG_H:		p_->cpu.h = val; break;
		case REG_L:		p_->cpu.l = val; break;
		default:		break;
	}
}

void GB::set_debug_buffer(debugbuffer& dbgbuf)
{
	p_->cpu.set_debug_buffer(dbgbuf);
}

uint8_t GB::bus_read(unsigned addr)
{
	return p_->cpu.bus_read(addr);
}

void GB::bus_write(unsigned addr, uint8_t val)
{
	p_->cpu.bus_write(addr, val);
}

void GB::set_emuflags(unsigned flags)
{
	p_->cpu.set_emuflags(flags);
}

}
