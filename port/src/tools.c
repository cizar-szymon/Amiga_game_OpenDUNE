/** @file src/tools.c Various routines. */

#include <stdio.h>
#include <stdlib.h>
#include "types.h"

#include "tools.h"
#include "house.h"
#include "timer.h"

#include "config.h"
#include "pool/structure.h"
#include "pool/unit.h"
#include "structure.h"
#include "tile.h"
#include "unit.h"


static uint8 s_randomSeed[4];
static uint32 s_randomLCG;

uint16 Tools_AdjustToGameSpeed(uint16 normal, uint16 minimum, uint16 maximum, bool inverseSpeed)
{
	uint16 gameSpeed = g_gameConfig.gameSpeed;

	if (gameSpeed == 2) return normal;
	if (gameSpeed > 4) return normal;

	if (maximum > normal * 2) maximum = normal * 2;
	if (minimum < normal / 2) minimum = normal / 2;

	if (inverseSpeed) gameSpeed = 4 - gameSpeed;

	switch (gameSpeed) {
		case 0: return minimum;
		case 1: return normal - (normal - minimum) / 2;
		case 3: return normal + (maximum - normal) / 2;
		case 4: return maximum;
	}

	/* Never reached, but avoids compiler errors */
	return normal;
}

/**
 * Get the type of the given encoded index.
 *
 * @param id The encoded index to get the type of.
 * @return The type
 */
IndexType Tools_Index_GetType(uint16 encoded)
{
	switch(encoded & 0xC000) {
		case 0x4000: return IT_UNIT;
		case 0x8000: return IT_STRUCTURE;
		case 0xC000: return IT_TILE;
		default:     return IT_NONE;
	}
}

/**
 * Decode the given encoded index.
 *
 * @param id The encoded index to decode.
 * @return The decoded index.
 */
uint16 Tools_Index_Decode(uint16 encoded)
{
	if (Tools_Index_GetType(encoded) == IT_TILE) return Tile_PackXY((encoded >> 1) & 0x3F, (encoded >> 8) & 0x3F);
	return encoded & 0x3FFF;
}

/**
 * Encode the given index.
 *
 * @param id The index to encode.
 * @param type The type of the encoded Index.
 * @return The encoded Index.
 */
uint16 Tools_Index_Encode(uint16 index, IndexType type)
{
	switch (type) {
		case IT_TILE: {
			uint16 ret;

			ret  = ((Tile_GetPackedX(index) << 1) + 1) << 0;
			ret |= ((Tile_GetPackedY(index) << 1) + 1) << 7;
			return ret | 0xC000;
		}
		case IT_UNIT: {
			if (index >= UNIT_INDEX_MAX || !Unit_Get_ByIndex(index)->o.flags.s.allocated) return 0;
			return index | 0x4000;
		}
		case IT_STRUCTURE:  return index | 0x8000;
		default: return 0;
	}
}

/**
 * Check whether an encoded index is valid.
 *
 * @param id The encoded index to check for validity.
 * @return True if valid, false if not.
 */
bool Tools_Index_IsValid(uint16 encoded)
{
	uint16 index;

	if (encoded == 0) return false;

	index = Tools_Index_Decode(encoded);

	switch (Tools_Index_GetType(encoded)) {
		case IT_UNIT:
			if (index >= UNIT_INDEX_MAX) return false;
			return Unit_Get_ByIndex(index)->o.flags.s.used && Unit_Get_ByIndex(index)->o.flags.s.allocated;

		case IT_STRUCTURE:
			if (index >= STRUCTURE_INDEX_MAX_HARD) return false;
			return Structure_Get_ByIndex(index)->o.flags.s.used;

		case IT_TILE : return true;

		default: return false;
	}
}

/**
 * Gets the packed tile corresponding to the given encoded index.
 *
 * @param id The encoded index to get the packed tile of.
 * @return The packed tile.
 */
uint16 Tools_Index_GetPackedTile(uint16 encoded)
{
	uint16 index;

	index = Tools_Index_Decode(encoded);

	switch (Tools_Index_GetType(encoded)) {
		case IT_TILE:      return index;
		case IT_UNIT:      return (index < UNIT_INDEX_MAX) ? Tile_PackTile(Unit_Get_ByIndex(index)->o.position) : 0;
		case IT_STRUCTURE: return (index < STRUCTURE_INDEX_MAX_HARD) ? Tile_PackTile(Structure_Get_ByIndex(index)->o.position) : 0;
		default:           return 0;
	}
}

/**
 * Gets the tile corresponding to the given encoded index.
 *
 * @param id The encoded index to get the tile of.
 * @return The tile.
 */
tile32 Tools_Index_GetTile(uint16 encoded)
{
	uint16 index;
	tile32 tile;

	index = Tools_Index_Decode(encoded);
	tile.x = 0;
	tile.y = 0;

	switch (Tools_Index_GetType(encoded)) {
		case IT_TILE: return Tile_UnpackTile(index);
		case IT_UNIT: return (index < UNIT_INDEX_MAX) ? Unit_Get_ByIndex(index)->o.position : tile;
		case IT_STRUCTURE: {
			const StructureInfo *si;
			Structure *s;

			if (index >= STRUCTURE_INDEX_MAX_HARD) return tile;

			s = Structure_Get_ByIndex(index);
			si = &g_table_structureInfo[s->o.type];

			return Tile_AddTileDiff(s->o.position, g_table_structure_layoutTileDiff[si->layout]);
		}
		default: return tile;
	}
}

/**
 * Gets the Unit corresponding to the given encoded index.
 *
 * @param id The encoded index to get the Unit of.
 * @return The Unit.
 */
Unit *Tools_Index_GetUnit(uint16 encoded)
{
	uint16 index;

	if (Tools_Index_GetType(encoded) != IT_UNIT) return NULL;

	index = Tools_Index_Decode(encoded);
	return (index < UNIT_INDEX_MAX) ? Unit_Get_ByIndex(index) : NULL;
}

/**
 * Gets the Structure corresponding to the given encoded index.
 *
 * @param id The encoded index to get the Structure of.
 * @return The Structure.
 */
Structure *Tools_Index_GetStructure(uint16 encoded)
{
	uint16 index;

	if (Tools_Index_GetType(encoded) != IT_STRUCTURE) return NULL;

	index = Tools_Index_Decode(encoded);
	return (index < STRUCTURE_INDEX_MAX_HARD) ? Structure_Get_ByIndex(index) : NULL;
}

/**
 * Gets the Object corresponding to the given encoded index.
 *
 * @param id The encoded index to get the Object of.
 * @return The Object.
 */
Object *Tools_Index_GetObject(uint16 encoded)
{
	uint16 index;

	switch (Tools_Index_GetType(encoded)) {
		case IT_UNIT:
			index = Tools_Index_Decode(encoded);
			return (index < UNIT_INDEX_MAX) ? &Unit_Get_ByIndex(index)->o : NULL;

		case IT_STRUCTURE:
			index = Tools_Index_Decode(encoded);
			return (index < STRUCTURE_INDEX_MAX_HARD) ? &Structure_Get_ByIndex(index)->o : NULL;

		default: return NULL;
	}
}

/**
 * Get a random value between 0 and 255.
 *
 * @return The random value.
 */
#if defined(__amigaos__)
/* tests (opendune.ini debug_lan_trace_from / _to, game ticks): who draws the
 * simulation's random numbers, in RAM:OpenDUNE-lan.txt (lan_amiga.c) */
uint32 g_lanTraceFrom = 0xFFFFFFFF, g_lanTraceTo = 0;
extern void Lan_TraceRandom(void *caller);

/* LAN game: the interface's random numbers (sounds, music) are its own */
static uint32 s_randomUI = 0x1234567;
static uint8 Tools_RandomUI(void)
{
	s_randomUI = s_randomUI * 1103515245UL + 12345;
	return (uint8)(s_randomUI >> 16);
}

uint8 Tools_RandomUI_256(void)
{
	return Tools_RandomUI();
}

uint16 Tools_RandomUI_Range(uint16 min, uint16 max)
{
	if (min > max) {
		uint16 t = min;
		min = max;
		max = t;
	}
	return (uint16)(min + ((Tools_RandomUI() << 8) | Tools_RandomUI()) % (max - min + 1));
}
#endif

uint8 Tools_Random_256(void)
{
	uint16 val16;
	uint8 val8;

#if defined(__amigaos__)
	if (LAN_UI) return Tools_RandomUI();
	if (g_lanGame && g_timerGame >= g_lanTraceFrom && g_timerGame <= g_lanTraceTo) Lan_TraceRandom(__builtin_return_address(0));
#endif

	val16 = (s_randomSeed[1] << 8) | s_randomSeed[2];
	val8 = ((val16 ^ 0x8000) >> 15) & 1;
	val16 = (val16 << 1) | ((s_randomSeed[0] >> 1) & 1);
	val8 = (s_randomSeed[0] >> 2) - s_randomSeed[0] - val8;
	s_randomSeed[0] = (val8 << 7) | (s_randomSeed[0] >> 1);
	s_randomSeed[1] = val16 >> 8;
	s_randomSeed[2] = val16 & 0xFF;

	return s_randomSeed[0] ^ s_randomSeed[1];
}

/**
 * Set the seed for the Tools_Random_256().
 * @param seed The seed to set the randomizer to.
 */
void Tools_Random_Seed(uint32 seed)
{
	s_randomSeed[0] = (seed >>  0) & 0xFF;
	s_randomSeed[1] = (seed >>  8) & 0xFF;
	s_randomSeed[2] = (seed >> 16) & 0xFF;
	s_randomSeed[3] = (seed >> 24) & 0xFF;
}

#if defined(__amigaos__)
/** The state of both random generators, for the LAN game's checksum. */
uint32 Tools_Random_State(void)
{
	return (((uint32)s_randomSeed[0] << 24) | ((uint32)s_randomSeed[1] << 16) | ((uint32)s_randomSeed[2] << 8)) ^ s_randomLCG;
}
#endif

/**
 * Set the seed for the LCG randomizer.
 */
#if defined(__amigaos__)
/** Both generators' state, 8 bytes (a LAN game's save: lan_amiga.c). */
void Tools_Random_GetFull(uint8 *b)
{
	b[0] = s_randomSeed[0]; b[1] = s_randomSeed[1]; b[2] = s_randomSeed[2]; b[3] = s_randomSeed[3];
	b[4] = (uint8)(s_randomLCG >> 24); b[5] = (uint8)(s_randomLCG >> 16);
	b[6] = (uint8)(s_randomLCG >> 8); b[7] = (uint8)s_randomLCG;
}

void Tools_Random_SetFull(const uint8 *b)
{
	s_randomSeed[0] = b[0]; s_randomSeed[1] = b[1]; s_randomSeed[2] = b[2]; s_randomSeed[3] = b[3];
	s_randomLCG = ((uint32)b[4] << 24) | ((uint32)b[5] << 16) | ((uint32)b[6] << 8) | b[7];
}
#endif

void Tools_RandomLCG_Seed(uint16 seed)
{
	s_randomLCG = seed;
}

/**
 * Get a random value from the LCG.
 */
static int16 Tools_RandomLCG(void)
{
	/* Borland C/C++ 'a' and 'b' value, bits 30..16, as used by Dune2 */
	s_randomLCG = 0x015A4E35 * s_randomLCG + 1;
	return (s_randomLCG >> 16) & 0x7FFF;
}

/**
 * Get a random value between the given values.
 *
 * @param min The minimum value.
 * @param max The maximum value.
 * @return The random value.
 */
uint16 Tools_RandomLCG_Range(uint16 min, uint16 max)
{
	uint16 ret;

#if defined(__amigaos__)
	if (LAN_UI) {
		if (min > max) { ret = min; min = max; max = ret; }
		return (uint16)(min + ((Tools_RandomUI() << 8) | Tools_RandomUI()) % (max - min + 1));
	}
	if (g_lanGame && g_timerGame >= g_lanTraceFrom && g_timerGame <= g_lanTraceTo) Lan_TraceRandom((void *)((uint32)__builtin_return_address(0) | 1));
#endif

	if (min > max) {
		uint16 temp = min;
		min = max;
		max = temp;
	}

	do {
		uint16 value = (int32)Tools_RandomLCG() * (max - min + 1) / 0x8000 + min;
		ret = value;
	} while (ret > max);

	return ret;
}

#ifndef BITARRAY_MACROS
/**
 * Test a bit in a bit array.
 * @param array Bit array.
 * @param index Index in the array.
 * @return value of the bit.
 */
bool BitArray_Test(uint8 *array, uint16 index)
{
	return (array[index >> 3] & (1 << (index & 7))) != 0;
}

/**
 * Set a bit in a bit array.
 * @param array Bit array.
 * @param index Index in the array.
 */
void BitArray_Set(uint8 *array, uint16 index)
{
	array[index >> 3] |= (1 << (index & 7));
}

/**
 * Clear a bit in a bit array.
 * @param array Bit array.
 * @param index Index in the array.
 */
void BitArray_Clear(uint8 *array, uint16 index)
{
	array[index >> 3] &= ~(1 << (index & 7));
}
#endif
