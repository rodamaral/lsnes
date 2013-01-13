#ifdef CORETYPE_MEDNAFEN
#include "interface/romtype.hpp"
#include "interface/callbacks.hpp"
#include "interface/cover.hpp"

namespace
{
	uint16_t cover_fbmem[512*448];
	//Framebuffer.
	struct framebuffer_info cover_fbinfo = {
		&_pixel_format_rgb16,		//Format.
		(char*)cover_fbmem,		//Memory.
		512, 448, 1024,			//Physical size.
		512, 448, 1024,			//Logical size.
		0, 0				//Offset.
	};

	core_setting_group mdfn_wswan_settings;
	core_setting_group mdfn_gba_settings;
	core_setting_group mdfn_nes_settings;
	core_setting_group mdfn_vboy_settings;
	core_setting_group mdfn_pcfx_settings;
	core_setting_group mdfn_ngp_settings;
	core_setting_group mdfn_pce_settings;
	core_setting_group mdfn_md_settings;

	/////////////////// SYSTEM CONTROLLER ///////////////////
	const char* system_name = "(system)";
	port_controller_button* system_button_info[] = {};
	port_controller system_controller = {"(system)", "system", 0, system_button_info};

	////////////////// NONE CONTROLLER ///////////////////
	const char* none_buttons = "";

	/////////////////// WSWAN CONTROLLER ///////////////////
	const char* wswan_buttons = "URDLurdlSAB";
	port_controller_button wswan_btn_U = {port_controller_button::TYPE_BUTTON, "xup"};
	port_controller_button wswan_btn_R = {port_controller_button::TYPE_BUTTON, "xright"};
	port_controller_button wswan_btn_D = {port_controller_button::TYPE_BUTTON, "xdown"};
	port_controller_button wswan_btn_L = {port_controller_button::TYPE_BUTTON, "xleft"};
	port_controller_button wswan_btn_u = {port_controller_button::TYPE_BUTTON, "yup"};
	port_controller_button wswan_btn_r = {port_controller_button::TYPE_BUTTON, "yright"};
	port_controller_button wswan_btn_d = {port_controller_button::TYPE_BUTTON, "ydown"};
	port_controller_button wswan_btn_l = {port_controller_button::TYPE_BUTTON, "yleft"};
	port_controller_button wswan_btn_S = {port_controller_button::TYPE_BUTTON, "start"};
	port_controller_button wswan_btn_A = {port_controller_button::TYPE_BUTTON, "A"};
	port_controller_button wswan_btn_B = {port_controller_button::TYPE_BUTTON, "B"};
	port_controller_button* wswan_button_info[] = {
		&wswan_btn_U, &wswan_btn_R, &wswan_btn_D, &wswan_btn_L,
		&wswan_btn_u, &wswan_btn_r, &wswan_btn_d, &wswan_btn_l,
		&wswan_btn_S, &wswan_btn_A, &wswan_btn_B
	};

	/////////////////// GBA CONTROLLER ///////////////////
	const char* gba_buttons = "ABsSrludLR";
	port_controller_button gba_btn_A = {port_controller_button::TYPE_BUTTON, "A"};
	port_controller_button gba_btn_B = {port_controller_button::TYPE_BUTTON, "B"};
	port_controller_button gba_btn_s = {port_controller_button::TYPE_BUTTON, "select"};
	port_controller_button gba_btn_S = {port_controller_button::TYPE_BUTTON, "start"};
	port_controller_button gba_btn_r = {port_controller_button::TYPE_BUTTON, "right"};
	port_controller_button gba_btn_l = {port_controller_button::TYPE_BUTTON, "left"};
	port_controller_button gba_btn_u = {port_controller_button::TYPE_BUTTON, "up"};
	port_controller_button gba_btn_d = {port_controller_button::TYPE_BUTTON, "down"};
	port_controller_button gba_btn_L = {port_controller_button::TYPE_BUTTON, "L"};
	port_controller_button gba_btn_R = {port_controller_button::TYPE_BUTTON, "R"};
	port_controller_button* gba_button_info[] = {
		&gba_btn_A, &gba_btn_B, &gba_btn_s, &gba_btn_S,
		&gba_btn_r, &gba_btn_l, &gba_btn_u, &gba_btn_d,
		&gba_btn_L, &gba_btn_R
	};

	/////////////////// NES GAMEPAD CONTROLLER ///////////////////
	const char* nesgp_buttons = "ABsSudlr";
	port_controller_button nesgp_btn_A = {port_controller_button::TYPE_BUTTON, "A"};
	port_controller_button nesgp_btn_B = {port_controller_button::TYPE_BUTTON, "B"};
	port_controller_button nesgp_btn_s = {port_controller_button::TYPE_BUTTON, "select"};
	port_controller_button nesgp_btn_S = {port_controller_button::TYPE_BUTTON, "start"};
	port_controller_button nesgp_btn_u = {port_controller_button::TYPE_BUTTON, "up"};
	port_controller_button nesgp_btn_d = {port_controller_button::TYPE_BUTTON, "down"};
	port_controller_button nesgp_btn_l = {port_controller_button::TYPE_BUTTON, "left"};
	port_controller_button nesgp_btn_r = {port_controller_button::TYPE_BUTTON, "right"};
	port_controller_button* nesgp_button_info[] = {
		&nesgp_btn_A, &nesgp_btn_B, &nesgp_btn_s, &nesgp_btn_S,
		&nesgp_btn_u, &nesgp_btn_d, &nesgp_btn_l, &nesgp_btn_r,
	};

	/////////////////// NES ZAPPER CONTROLLER ///////////////////
	const char* neszp_buttons = "Ta";
	port_controller_button neszp_axis_x = {port_controller_button::TYPE_AXIS, "xaxis"};
	port_controller_button neszp_axis_y = {port_controller_button::TYPE_AXIS, "yaxis"};
	port_controller_button neszp_btn_T = {port_controller_button::TYPE_BUTTON, "trigger"};
	port_controller_button neszp_btn_a = {port_controller_button::TYPE_BUTTON, "awaytrigger"};
	port_controller_button* neszp_button_info[] = {
		&neszp_axis_x, &neszp_axis_y, &neszp_btn_T, &neszp_btn_a
	};


#endif
