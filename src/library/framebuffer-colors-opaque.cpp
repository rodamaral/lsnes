#include "framebuffer.hpp"

namespace
{
	framebuffer::color_mod opaque10("opaque10", [](int64_t& v) { v = (230ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque20("opaque20", [](int64_t& v) { v = (205ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque25("opaque25", [](int64_t& v) { v = (192ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque30("opaque30", [](int64_t& v) { v = (179ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque40("opaque40", [](int64_t& v) { v = (154ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque50("opaque50", [](int64_t& v) { v = (128ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque60("opaque60", [](int64_t& v) { v = (102ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque70("opaque70", [](int64_t& v) { v = (77ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque75("opaque75", [](int64_t& v) { v = (64ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque80("opaque80", [](int64_t& v) { v = (51ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque90("opaque90", [](int64_t& v) { v = (26ULL << 24) | (v & 0xFFFFFF); });
	framebuffer::color_mod opaque("opaque", [](int64_t& v) { v = (0ULL << 24) | (v & 0xFFFFFF); });
}
