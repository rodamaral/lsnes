#ifndef _lua__address__hpp__included__
#define _lua__address__hpp__included__

namespace lua { class parameters; }
namespace lua { class state; }

uint64_t lua_get_vmabase(const text& vma);
uint64_t lua_get_read_address(lua::parameters& P);

class lua_address
{
public:
	lua_address(lua::state& L);
	static int create(lua::state& L, lua::parameters& P);
	text print();
	uint64_t get();
	text get_vma();
	uint64_t get_offset();
	int l_get(lua::state& L, lua::parameters& P);
	int l_get_vma(lua::state& L, lua::parameters& P);
	int l_get_offset(lua::state& L, lua::parameters& P);
	int l_shift(lua::state& L, lua::parameters& P);
	int l_replace(lua::state& L, lua::parameters& P);
	static size_t overcommit();
	template<class T, bool _bswap> int rw(lua::state& L, lua::parameters& P);
private:
	text vma;
	uint64_t addr;
};

#endif
