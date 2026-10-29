/* LAN maps from SCENARIO.PAK (tools/lan_maps.py): the landscape of a
 * scenario and three of its bases far apart; each player starts at one with
 * a Construction Yard and units where the base had units (else next to it). */
typedef struct LanMap { const char *name; uint16 seed; const char *field, *bloom; uint16 cy[3]; uint16 units[3][4]; } LanMap;
static const LanMap s_lanMaps[] = {
	{ "SCENA020", 604, "2746,3482,3791", "", { 3294, 2425, 666 }, { { 3470, 3111, 3079, 3092 }, { 2362, 2225, 1904, 1786 }, { 794, 796, 728, 599 } } },
	{ "SCENA022", 392, "83,130,156,177,231,381,567", "983,1762,2015,3343,3383", { 122, 73, 1204 }, { { 814, 482, 352, 356 }, { 531, 533, 203, 205 }, { 1209, 1014, 821, 1334 } } },
	{ "SCENH021", 723, "3691", "", { 3680, 171, 2009 }, { { 3603, 3614, 3432, 3321 }, { 745, 482, 439, 351 }, { 2136, 2139, 2070, 2011 } } },
};
