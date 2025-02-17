/*
 * Wolfenstein: Enemy Territory GPL Source Code
 * Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
 *
 * ET: Legacy
 * Copyright (C) 2012-2024 ET:Legacy team <mail@etlegacy.com>
 *
 * This file is part of ET: Legacy - http://www.etlegacy.com
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ET: Legacy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ET: Legacy. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, Wolfenstein: Enemy Territory GPL Source Code is also
 * subject to certain additional terms. You should have received a copy
 * of these additional terms immediately following the terms and conditions
 * of the GNU General Public License which accompanied the source code.
 * If not, please request a copy in writing from id Software at the address below.
 *
 * id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
 */
/**
 * @file msg.c
 */

#include "q_shared.h"
#include "qcommon.h"

// FIXME: necessary for entityShared_t management to work (since we need the definitions...),
// which is a very necessary function for server-side demos recording. It would be better if this
// functionality would be separated in an _ext.c file, but I could not find a way to make it work
// (because it also needs the definitions in msg.c, and since it's not a header, these are being
// redefined when included, producing a lot of recursive declarations errors...)
#include "../game/g_public.h"

static huffman_t msgHuff;
static qboolean  msgInit = qfalse;

int pcount[256];
int wastedbits = 0;

static int oldsize = 0;

/*
==============================================================================
            MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

void MSG_initHuffman(void);

/**
 * @brief MSG_Init
 * @param[out] buf
 * @param[in] data
 * @param[in] length
 */
void MSG_Init(msg_t *buf, byte *data, int length)
{
	if (!msgInit)
	{
		MSG_initHuffman();
	}
	Com_Memset(buf, 0, sizeof(*buf));
	// optimization
	//Com_Memset (data, 0, length);
	buf->data    = data;
	buf->maxsize = length;
}

/**
 * @brief MSG_InitOOB
 * @param[out] buf
 * @param[in] data
 * @param[in] length
 */
void MSG_InitOOB(msg_t *buf, byte *data, int length)
{
	if (!msgInit)
	{
		MSG_initHuffman();
	}
	Com_Memset(buf, 0, sizeof(*buf));
	// optimization
	//Com_Memset (data, 0, length);
	buf->data    = data;
	buf->maxsize = length;
	buf->oob     = qtrue;
}

/**
 * @brief MSG_Clear
 * @param[out] buf
 */
void MSG_Clear(msg_t *buf)
{
	buf->cursize    = 0;
	buf->overflowed = qfalse;
	buf->bit        = 0;            //<- in bits
}

/**
 * @brief MSG_Bitstream
 * @param[out] buf
 */
void MSG_Bitstream(msg_t *buf)
{
	buf->oob = qfalse;
}

/**
 * @brief MSG_Uncompressed
 * @param[out] buf
 */
void MSG_Uncompressed(msg_t *buf)
{
	// align to byte-boundary
	buf->bit = (buf->bit + 7) & ~7;
	buf->oob = qtrue;
}

/**
 * @brief MSG_BeginReading
 * @param[out] msg
 */
void MSG_BeginReading(msg_t *msg)
{
	msg->readcount = 0;
	msg->bit       = 0;
	msg->oob       = qfalse;
}

void MSG_BeginReadingOOB(msg_t *msg)
{
	msg->readcount = 0;
	msg->bit       = 0;
	msg->oob       = qtrue;
}

/**
 * @brief MSG_BeginReadingUncompressed
 * @param[out] msg
 */
void MSG_BeginReadingUncompressed(msg_t *msg)
{
	// align to byte-boundary
	msg->bit = (msg->bit + 7) & ~7;
	msg->oob = qtrue;
}

/**
 * @brief MSG_Copy
 * @param[out] buf
 * @param[in] data
 * @param[in] length
 * @param[in] src
 */
void MSG_Copy(msg_t *buf, byte *data, int length, msg_t *src)
{
	if (length < src->cursize)
	{
		Com_Error(ERR_DROP, "MSG_Copy: can't copy %d (%i) into a smaller %d msg_t buffer", src->cursize, src->bit, length);
	}
	Com_Memcpy(buf, src, sizeof(msg_t));
	buf->data = data;
	Com_Memcpy(buf->data, src->data, src->cursize);
}

/*
=============================================================================
bit functions
=============================================================================
*/

// Negative bit values include signs

/**
 * @brief MSG_WriteBits
 * @param[in,out] msg
 * @param[in] value
 * @param[in] bits
 */
void MSG_WriteBits(msg_t *msg, int value, int bits)
{
	oldsize += bits;

	msg->uncompsize += bits; // net debugging

	if (msg->overflowed)
	{
		return;
	}

	if (bits == 0 || bits < -31 || bits > 32)
	{
		Com_Error(ERR_DROP, "MSG_WriteBits: bad bits %i", bits);
	}

	if (bits < 0)
	{
		bits = -bits;
	}

	if (msg->oob)
	{
		if (msg->cursize + (bits >> 3) > msg->maxsize)
		{
			msg->overflowed = qtrue;
			return;
		}

		switch (bits)
		{
		case 8:
			msg->data[msg->cursize] = value;
			msg->cursize           += 1;
			msg->bit               += 8;
			break;
		case 16:
		{
			uint16_t *sp = (uint16_t *)&msg->data[msg->cursize];

			*sp           = LittleShort(value);
			msg->cursize += 2;
			msg->bit     += 16;
		}
		break;
		case 32:
		{
			uint32_t *ip = (uint32_t *)&msg->data[msg->cursize];

			*ip           = LittleLong(value);
			msg->cursize += 4;
			msg->bit     += 32;
		}
		break;
		default:
			Com_Error(ERR_DROP, "MSG_WriteBits: can't read %d bits", bits);
			break;
		}
	}
	else
	{
		int i;

		value &= (0xffffffff >> (32 - bits));
		if (bits & 7)
		{
			int nbits = bits & 7;

			if (msg->bit + nbits > msg->maxsize << 3)
			{
				msg->overflowed = qtrue;
				return;
			}

			for (i = 0; i < nbits; i++)
			{
				Huff_putBit((value & 1), msg->data, &msg->bit);
				value = (value >> 1);
			}
			bits = bits - nbits;
		}
		if (bits)
		{
			for (i = 0; i < bits; i += 8)
			{
				Huff_offsetTransmit(&msgHuff.compressor, (value & 0xff), msg->data, &msg->bit, msg->maxsize << 3);
				value = (value >> 8);

				if (msg->bit >= msg->maxsize << 3)
				{
					msg->overflowed = qtrue;
					return;
				}
			}
		}
		msg->cursize = (msg->bit >> 3) + 1;
	}
}

/**
 * @brief MSG_ReadBits
 * @param[in,out] msg
 * @param[in] bits
 * @return
 */
int MSG_ReadBits(msg_t *msg, int bits)
{
	int      value = 0;
	qboolean sgn;

	if (msg->readcount > msg->cursize)
	{
		return 0;
	}

	if (bits < 0)
	{
		bits = -bits;
		sgn  = qtrue;
	}
	else
	{
		sgn = qfalse;
	}

	if (msg->oob)
	{
		if (msg->readcount + (bits >> 3) > msg->cursize)
		{
			msg->readcount = msg->cursize + 1;
			return 0;
		}

		switch (bits)
		{
		case 8:
			value           = msg->data[msg->readcount];
			msg->readcount += 1;
			msg->bit       += 8;
			break;
		case 16:
		{
			unsigned short *sp = (unsigned short *)&msg->data[msg->readcount];

			value           = LittleShort(*sp);
			msg->readcount += 2;
			msg->bit       += 16;
		}
		break;
		case 32:
		{
			unsigned int *ip = (unsigned int *)&msg->data[msg->readcount];

			value           = LittleLong(*ip);
			msg->readcount += 4;
			msg->bit       += 32;
		}
		break;
		default:
			Com_Error(ERR_DROP, "MSG_ReadBits: can't read %d bits", bits);
			break;
		}
	}
	else
	{
		int i, nbits = 0;

		if (bits & 7)
		{
			nbits = bits & 7;

			if (msg->bit + nbits > msg->cursize << 3)
			{
				msg->readcount = msg->cursize + 1;
				return 0;
			}

			for (i = 0; i < nbits; i++)
			{
				value |= (Huff_getBit(msg->data, &msg->bit) << i);
			}
			bits = bits - nbits;
		}
		if (bits)
		{
			int get;

			for (i = 0; i < bits; i += 8)
			{
				Huff_offsetReceive(msgHuff.decompressor.tree, &get, msg->data, &msg->bit, msg->cursize << 3);
				value = (unsigned int)value | ((unsigned int)get << (i + nbits));

				if (msg->bit > msg->cursize << 3)
				{
					msg->readcount = msg->cursize + 1;
					return 0;
				}
			}
		}
		msg->readcount = (msg->bit >> 3) + 1;
	}
	if (sgn && bits > 0 && bits < 32)
	{
		if (value & (1 << (bits - 1)))
		{
			value |= -1 ^ ((1 << bits) - 1);
		}
	}

	return value;
}

//================================================================================

// writing functions

/**
 * @brief MSG_WriteChar
 * @param[in,out] msg
 * @param[in] c
 */
void MSG_WriteChar(msg_t *msg, int c)
{
#ifdef PARANOID
	if (c < -128 || c > 127)
	{
		Com_Error(ERR_FATAL, "MSG_WriteChar: range error");
	}
#endif

	MSG_WriteBits(msg, c, 8);
}

/**
 * @brief MSG_WriteByte
 * @param[in,out] msg
 * @param[in] c
 */
void MSG_WriteByte(msg_t *msg, int c)
{
#ifdef PARANOID
	if (c < 0 || c > 255)
	{
		Com_Error(ERR_FATAL, "MSG_WriteByte: range error");
	}
#endif

	MSG_WriteBits(msg, c, 8);
}

/**
 * @brief MSG_WriteData
 * @param[in,out] buf
 * @param[in] data
 * @param[in] length
 */
void MSG_WriteData(msg_t *buf, const void *data, int length)
{
	int i;

	for (i = 0; i < length; i++)
	{
		MSG_WriteByte(buf, ((const byte *)data)[i]);
	}
}

/**
 * @brief MSG_WriteShort
 * @param[in,out] msg
 * @param[in] c
 */
void MSG_WriteShort(msg_t *msg, int c)
{
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
	{
		Com_Error(ERR_FATAL, "MSG_WriteShort: range error");
	}
#endif

	MSG_WriteBits(msg, c, 16);
}

/**
 * @brief MSG_WriteLong
 * @param[in,out] msg
 * @param[in] c
 */
void MSG_WriteLong(msg_t *msg, int c)
{
	MSG_WriteBits(msg, c, 32);
}

/**
 * @brief MSG_WriteFloat
 * @param[in,out] msg
 * @param[in] f
 */
void MSG_WriteFloat(msg_t *msg, float f)
{
	union
	{
		float f;
		int l;
	} dat;

	dat.f = f;
	MSG_WriteBits(msg, dat.l, 32);
}

/**
 * @brief MSG_WriteString
 * @param[in,out] msg
 * @param[in] s
 */
void MSG_WriteString(msg_t *msg, const char *s)
{
	if (!s)
	{
		MSG_WriteData(msg, "", 1);
	}
	else
	{
		int  l;
		char string[MAX_STRING_CHARS];

		l = strlen(s);
		if (l >= MAX_STRING_CHARS)
		{
			Com_Printf("MSG_WriteString: MAX_STRING_CHARS size reached\n");
			MSG_WriteData(msg, "", 1);
			return;
		}
		Q_strncpyz(string, s, sizeof(string));

		Q_SafeNetString(string, l, msg->strip);

		MSG_WriteData(msg, string, l + 1);
	}
}

/**
 * @brief MSG_WriteBigString
 * @param[in,out] msg
 * @param[in] s
 */
void MSG_WriteBigString(msg_t *msg, const char *s)
{
	if (!s)
	{
		MSG_WriteData(msg, "", 1);
	}
	else
	{
		int  l;
		char string[BIG_INFO_STRING];

		l = strlen(s);
		if (l >= BIG_INFO_STRING)
		{
			Com_Printf("MSG_WriteString: BIG_INFO_STRING size reached\n");
			MSG_WriteData(msg, "", 1);
			return;
		}
		Q_strncpyz(string, s, sizeof(string));

		Q_SafeNetString(string, l, msg->strip);

		MSG_WriteData(msg, string, l + 1);
	}
}

/**
 * @brief MSG_WriteAngle
 * @param[in,out] msg
 * @param[in] f
 */
void MSG_WriteAngle(msg_t *msg, float f)
{
	MSG_WriteByte(msg, (int)(f * 256 / 360) & 255);
}

/**
 * @brief MSG_WriteAngle16
 * @param[in,out] msg
 * @param[in] f
 */
void MSG_WriteAngle16(msg_t *msg, float f)
{
	MSG_WriteShort(msg, ANGLE2SHORT(f));
}

// a string hasher which gives the same hash value even if the
// string is later modified via the legacy MSG read/write code
int MSG_HashKey(const char *string, int maxlen, int strip)
{
	int hash = 0, i;

	for (i = 0; i < maxlen && string[i] != '\0'; i++)
	{
		if ((strip && (string[i] & 0x80)) || string[i] == '%')
		{
			hash += '.' * (119 + i);
		}
		else
		{
			hash += string[i] * (119 + i);
		}
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	return hash;
}

//============================================================

// reading functions

/**
 * @brief MSG_ReadChar
 * @param[in] msg
 * @return -1 if no more characters are available
 */
int MSG_ReadChar(msg_t *msg)
{
	int c;

	c = (signed char)MSG_ReadBits(msg, 8);
	if (msg->readcount > msg->cursize)
	{
		c = -1;
	}

	return c;
}

/**
 * @brief MSG_ReadByte
 * @param[in] msg
 * @return -1 if no more characters are available
 */
int MSG_ReadByte(msg_t *msg)
{
	int c;

	c = (unsigned char)MSG_ReadBits(msg, 8);
	if (msg->readcount > msg->cursize)
	{
		c = -1;
	}
	return c;
}

/**
 * @brief MSG_ReadShort
 * @param[in] msg
 * @return -1 if no more characters are available
 */
int MSG_ReadShort(msg_t *msg)
{
	int c;

	c = (short)MSG_ReadBits(msg, 16);
	if (msg->readcount > msg->cursize)
	{
		c = -1;
	}

	return c;
}

/**
 * @brief MSG_ReadLong
 * @param[in] msg
 * @return -1 if no more characters are available
 */
int MSG_ReadLong(msg_t *msg)
{
	int c;

	c = MSG_ReadBits(msg, 32);
	if (msg->readcount > msg->cursize)
	{
		c = -1;
	}

	return c;
}

/**
 * @brief MSG_ReadFloat
 * @param[in] msg
 * @return -1 if no more characters are available
 */
float MSG_ReadFloat(msg_t *msg)
{
	union
	{
		byte b[4];
		float f;
		int l;
	} dat;

	dat.l = MSG_ReadBits(msg, 32);
	if (msg->readcount > msg->cursize)
	{
		dat.f = -1;
	}

	return dat.f;
}

/**
 * @brief MSG_ReadString
 * @param[in] msg
 * @return
 */
char *MSG_ReadString(msg_t *msg)
{
	static char  string[MAX_STRING_CHARS];
	unsigned int l = 0;
	int          c;

	do
	{
		c = MSG_ReadByte(msg); // use ReadByte so -1 is out of bounds
		if (c == -1 || c == 0)
		{
			break;
		}

		// translate all '%' fmt spec to avoid crash bugs
		// don't allow higher ascii values
		if ((msg->strip && (c & 0x80)) || c == '%')
		{
			c = '.';
		}

		// break only after reading all expected data from bitstream
		if (l >= sizeof(string) - 1)
		{
			break;
		}

		string[l++] = c;
	}
	while (1);

	string[l] = '\0';

	return string;
}

/**
 * @brief MSG_ReadBigString
 * @param[in] msg
 * @return
 */
char *MSG_ReadBigString(msg_t *msg)
{
	static char  string[BIG_INFO_STRING];
	unsigned int l = 0;
	int          c;

	do
	{
		c = MSG_ReadByte(msg); // use ReadByte so -1 is out of bounds
		if (c == -1 || c == 0)
		{
			break;
		}

		// translate all '%' fmt spec to avoid crash bugs
		// don't allow higher ascii values
		if ((msg->strip && (c & 0x80)) || c == '%')
		{
			c = '.';
		}

		// break only after reading all expected data from bitstream
		if (l >= sizeof(string) - 1)
		{
			break;
		}
		string[l++] = c;
	}
	while (1);

	string[l] = '\0';

	return string;
}

/**
 * @brief MSG_ReadStringLine
 * @param[in] msg
 * @return
 */
char *MSG_ReadStringLine(msg_t *msg)
{
	static char  string[MAX_STRING_CHARS];
	unsigned int l = 0;
	int          c;

	do
	{
		c = MSG_ReadByte(msg);        // use ReadByte so -1 is out of bounds
		if (c == -1 || c == 0 || c == '\n')
		{
			break;
		}

		// translate all '%' fmt spec to avoid crash bugs
		// don't allow higher ascii values
		if ((msg->strip && (c & 0x80)) || c == '%')
		{
			c = '.';
		}

		// break only after reading all expected data from bitstream
		if (l >= sizeof(string) - 1)
		{
			break;
		}
		string[l++] = c;
	}
	while (1);

	string[l] = '\0';

	return string;
}

/**
 * @brief MSG_ReadAngle16
 * @param[in] msg
 * @return
 */
float MSG_ReadAngle16(msg_t *msg)
{
	return SHORT2ANGLE(MSG_ReadShort(msg));
}

/**
 * @brief MSG_ReadData
 * @param[in] msg
 * @param[in] data
 * @param[in] size
 */
void MSG_ReadData(msg_t *msg, void *data, int size)
{
	int i;

	for (i = 0 ; i < size ; i++)
	{
		((byte *)data)[i] = MSG_ReadByte(msg);
	}
}

extern cvar_t *cl_shownet;

#define LOG(x) if (cl_shownet && cl_shownet->integer == 4) { Com_Printf("%s ", x); };

/*
=============================================================================
delta functions with keys
=============================================================================
*/

int kbitmask[32] =
{
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
	0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
	0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
	0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
	0x001FFFFf, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
	0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
};

/**
 * @brief MSG_WriteDeltaKey
 * @param[out] msg
 * @param[in] key
 * @param[in] oldV
 * @param[in] newV
 * @param[in] bits
 */
void MSG_WriteDeltaKey(msg_t *msg, int key, int oldV, int newV, int bits)
{
	if (oldV == newV)
	{
		MSG_WriteBits(msg, 0, 1);
		return;
	}
	MSG_WriteBits(msg, 1, 1);
	MSG_WriteBits(msg, newV ^ key, bits);
}

/**
 * @brief MSG_ReadDeltaKey
 * @param[in] msg
 * @param[in] key
 * @param[in] oldV
 * @param[in] bits
 * @return
 */
int MSG_ReadDeltaKey(msg_t *msg, int key, int oldV, int bits)
{
	if (MSG_ReadBits(msg, 1))
	{
		return MSG_ReadBits(msg, bits) ^ (key & kbitmask[bits - 1]);
	}
	return oldV;
}

/**
 * @brief MSG_WriteDeltaKeyFloat
 * @param[out] msg
 * @param[in] key
 * @param[in] oldV
 * @param[in] newV
 */
void MSG_WriteDeltaKeyFloat(msg_t *msg, int key, float oldV, float newV)
{
	floatint_t fi;

	if (oldV == newV)
	{
		MSG_WriteBits(msg, 0, 1);
		return;
	}
	fi.f = newV;
	MSG_WriteBits(msg, 1, 1);
	MSG_WriteBits(msg, fi.i ^ key, 32);
}

/**
 * @brief MSG_ReadDeltaKeyFloat
 * @param[in] msg
 * @param[in] key
 * @param[in] oldV
 * @return
 */
float MSG_ReadDeltaKeyFloat(msg_t *msg, int key, float oldV)
{
	if (MSG_ReadBits(msg, 1))
	{
		floatint_t fi;

		fi.i = MSG_ReadBits(msg, 32) ^ key;
		return fi.f;
	}
	return oldV;
}

/*
============================================================================
usercmd_t communication
============================================================================
*/

/**
 * @brief MSG_WriteDeltaUsercmdKey
 * @param[out] msg
 * @param[in] key
 * @param[in] from
 * @param[in] to
 */
void MSG_WriteDeltaUsercmdKey(msg_t *msg, int key, usercmd_t *from, usercmd_t *to)
{
	if (to->serverTime - from->serverTime < 256)
	{
		MSG_WriteBits(msg, 1, 1);
		MSG_WriteBits(msg, to->serverTime - from->serverTime, 8);
	}
	else
	{
		MSG_WriteBits(msg, 0, 1);
		MSG_WriteBits(msg, to->serverTime, 32);
	}
	if (from->angles[0] == to->angles[0] &&
	    from->angles[1] == to->angles[1] &&
	    from->angles[2] == to->angles[2] &&
	    from->forwardmove == to->forwardmove &&
	    from->rightmove == to->rightmove &&
	    from->upmove == to->upmove &&
	    from->buttons == to->buttons &&
	    from->wbuttons == to->wbuttons &&
	    from->weapon == to->weapon &&
	    from->flags == to->flags &&
	    from->doubleTap == to->doubleTap &&
	    from->identClient == to->identClient)
	{
		MSG_WriteBits(msg, 0, 1); // no change
		oldsize += 7;
		return;
	}
	key ^= to->serverTime;
	MSG_WriteBits(msg, 1, 1);
	MSG_WriteDeltaKey(msg, key, from->angles[0], to->angles[0], 16);
	MSG_WriteDeltaKey(msg, key, from->angles[1], to->angles[1], 16);
	MSG_WriteDeltaKey(msg, key, from->angles[2], to->angles[2], 16);
	MSG_WriteDeltaKey(msg, key, from->forwardmove, to->forwardmove, 8);
	MSG_WriteDeltaKey(msg, key, from->rightmove, to->rightmove, 8);
	MSG_WriteDeltaKey(msg, key, from->upmove, to->upmove, 8);
	MSG_WriteDeltaKey(msg, key, from->buttons, to->buttons, 8);
	MSG_WriteDeltaKey(msg, key, from->wbuttons, to->wbuttons, 8);
	MSG_WriteDeltaKey(msg, key, from->weapon, to->weapon, 8);
	MSG_WriteDeltaKey(msg, key, from->flags, to->flags, 8);
	MSG_WriteDeltaKey(msg, key, from->doubleTap, to->doubleTap, 3);
	MSG_WriteDeltaKey(msg, key, from->identClient, to->identClient, 8);
}

/**
 * @brief MSG_ReadDeltaUsercmdKey
 * @param[in] msg
 * @param[in] key
 * @param[in] from
 * @param[out] to
 */
void MSG_ReadDeltaUsercmdKey(msg_t *msg, int key, usercmd_t *from, usercmd_t *to)
{
	if (MSG_ReadBits(msg, 1))
	{
		to->serverTime = from->serverTime + MSG_ReadBits(msg, 8);
	}
	else
	{
		to->serverTime = MSG_ReadBits(msg, 32);
	}
	if (MSG_ReadBits(msg, 1))
	{
		key          ^= to->serverTime;
		to->angles[0] = MSG_ReadDeltaKey(msg, key, from->angles[0], 16);
		to->angles[1] = MSG_ReadDeltaKey(msg, key, from->angles[1], 16);
		to->angles[2] = MSG_ReadDeltaKey(msg, key, from->angles[2], 16);

		// disallow moves of -128 (speedhack)
		to->forwardmove = MSG_ReadDeltaKey(msg, key, from->forwardmove, 8);
		if (to->forwardmove == -128)
		{
			to->forwardmove = -127;
		}
		to->rightmove = MSG_ReadDeltaKey(msg, key, from->rightmove, 8);
		if (to->rightmove == -128)
		{
			to->rightmove = -127;
		}
		to->upmove = MSG_ReadDeltaKey(msg, key, from->upmove, 8);
		if (to->upmove == -128)
		{
			to->upmove = -127;
		}

		to->buttons     = MSG_ReadDeltaKey(msg, key, from->buttons, 8);
		to->wbuttons    = MSG_ReadDeltaKey(msg, key, from->wbuttons, 8);
		to->weapon      = MSG_ReadDeltaKey(msg, key, from->weapon, 8);
		to->flags       = MSG_ReadDeltaKey(msg, key, from->flags, 8);
		to->doubleTap   = MSG_ReadDeltaKey(msg, key, from->doubleTap, 3) & 0x7;
		to->identClient = MSG_ReadDeltaKey(msg, key, from->identClient, 8);
	}
	else
	{
		to->angles[0]   = from->angles[0];
		to->angles[1]   = from->angles[1];
		to->angles[2]   = from->angles[2];
		to->forwardmove = from->forwardmove;
		to->rightmove   = from->rightmove;
		to->upmove      = from->upmove;
		to->buttons     = from->buttons;
		to->wbuttons    = from->wbuttons;
		to->weapon      = from->weapon;
		to->flags       = from->flags;
		to->doubleTap   = from->doubleTap;
		to->identClient = from->identClient;
	}
}

/*
=============================================================================
entityState_t communication
=============================================================================
*/

/**
 * @brief Prints out a table from the current statistics for copying to code
 */
void MSG_ReportChangeVectors_f(void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		if (pcount[i])
		{
			Com_Printf("%d used %d\n", i, pcount[i]);
		}
	}
}

typedef struct
{
	char *name;
	int offset;
	int bits;           // 0 = float
	int used;
} netField_t;

/**
 * @brief Using the stringizing operator to save typing...
 */
#define NETF(x) # x, (size_t)&((entityState_t *)0)->x

netField_t entityStateFields[] =
{
	{ NETF(eType),           8,               0 },
	{ NETF(eFlags),          24,              0 },
	{ NETF(pos.trType),      8,               0 },
	{ NETF(pos.trTime),      32,              0 },
	{ NETF(pos.trDuration),  32,              0 },
	{ NETF(pos.trBase[0]),   0,               0 },
	{ NETF(pos.trBase[1]),   0,               0 },
	{ NETF(pos.trBase[2]),   0,               0 },
	{ NETF(pos.trDelta[0]),  0,               0 },
	{ NETF(pos.trDelta[1]),  0,               0 },
	{ NETF(pos.trDelta[2]),  0,               0 },
	{ NETF(apos.trType),     8,               0 },
	{ NETF(apos.trTime),     32,              0 },
	{ NETF(apos.trDuration), 32,              0 },
	{ NETF(apos.trBase[0]),  0,               0 },
	{ NETF(apos.trBase[1]),  0,               0 },
	{ NETF(apos.trBase[2]),  0,               0 },
	{ NETF(apos.trDelta[0]), 0,               0 },
	{ NETF(apos.trDelta[1]), 0,               0 },
	{ NETF(apos.trDelta[2]), 0,               0 },
	{ NETF(time),            32,              0 },
	{ NETF(time2),           32,              0 },
	{ NETF(origin[0]),       0,               0 },
	{ NETF(origin[1]),       0,               0 },
	{ NETF(origin[2]),       0,               0 },
	{ NETF(origin2[0]),      0,               0 },
	{ NETF(origin2[1]),      0,               0 },
	{ NETF(origin2[2]),      0,               0 },
	{ NETF(angles[0]),       0,               0 },
	{ NETF(angles[1]),       0,               0 },
	{ NETF(angles[2]),       0,               0 },
	{ NETF(angles2[0]),      0,               0 },
	{ NETF(angles2[1]),      0,               0 },
	{ NETF(angles2[2]),      0,               0 },
	{ NETF(otherEntityNum),  GENTITYNUM_BITS, 0 },
	{ NETF(otherEntityNum2), GENTITYNUM_BITS, 0 },
	{ NETF(groundEntityNum), GENTITYNUM_BITS, 0 },
	{ NETF(loopSound),       8,               0 },
	{ NETF(constantLight),   32,              0 },
	{ NETF(dl_intensity),    32,              0 }, // longer now to carry the corona colors
	{ NETF(modelindex),      9,               0 },
	{ NETF(modelindex2),     9,               0 },
	{ NETF(frame),           16,              0 },
	{ NETF(clientNum),       8,               0 },
	{ NETF(solid),           24,              0 },
	{ NETF(event),           10,              0 },
	{ NETF(eventParm),       8,               0 },
	{ NETF(eventSequence),   8,               0 }, // warning: need to modify cg_event.c at "// check the sequencial list" if you change this
	{ NETF(events[0]),       8,               0 },
	{ NETF(events[1]),       8,               0 },
	{ NETF(events[2]),       8,               0 },
	{ NETF(events[3]),       8,               0 },
	{ NETF(eventParms[0]),   8,               0 },
	{ NETF(eventParms[1]),   8,               0 },
	{ NETF(eventParms[2]),   8,               0 },
	{ NETF(eventParms[3]),   8,               0 },
	{ NETF(powerups),        16,              0 },
	{ NETF(weapon),          8,               0 },
	{ NETF(legsAnim),        ANIM_BITS,       0 },
	{ NETF(torsoAnim),       ANIM_BITS,       0 },
	{ NETF(density),         10,              0 },
	{ NETF(dmgFlags),        32,              0 }, // additional info flags for damage
	{ NETF(onFireStart),     32,              0 },
	{ NETF(onFireEnd),       32,              0 },
	{ NETF(nextWeapon),      8,               0 },
	{ NETF(teamNum),         8,               0 },
	{ NETF(effect1Time),     32,              0 },
	{ NETF(effect2Time),     32,              0 },
	{ NETF(effect3Time),     32,              0 },
	{ NETF(animMovetype),    4,               0 },
	{ NETF(aiState),         2,               0 },
};

/**
 * @brief qsort_entitystatefields
 * @param[in] a
 * @param[in] b
 * @return
 */
static int QDECL qsort_entitystatefields(const void *a, const void *b)
{
	const int aa = *((const int *)a);
	const int bb = *((const int *)b);

	if (entityStateFields[aa].used > entityStateFields[bb].used)
	{
		return -1;
	}
	if (entityStateFields[bb].used > entityStateFields[aa].used)
	{
		return 1;
	}
	return 0;
}

/**
 * @brief MSG_PrioritiseEntitystateFields
 */
void MSG_PrioritiseEntitystateFields(void)
{
	int fieldorders[sizeof(entityStateFields) / sizeof(entityStateFields[0])];
	int numfields = sizeof(entityStateFields) / sizeof(entityStateFields[0]);
	int i;

	for (i = 0; i < numfields; i++)
	{
		fieldorders[i] = i;
	}

	qsort(fieldorders, numfields, sizeof(int), qsort_entitystatefields);

	Com_Printf("Entitystate fields in order of priority\n");
	Com_Printf("netField_t entityStateFields[] = {\n");
	for (i = 0; i < numfields; i++)
	{
		Com_Printf("{ NETF (%s), %i },\n", entityStateFields[fieldorders[i]].name, entityStateFields[fieldorders[i]].bits);
	}
	Com_Printf("};\n");
}

// if (int)f == f and (int)f + ( 1<<(FLOAT_INT_BITS-1) ) < ( 1 << FLOAT_INT_BITS )
// the float will be sent with FLOAT_INT_BITS, otherwise all 32 bits will be sent
#define FLOAT_INT_BITS  13
#define FLOAT_INT_BIAS  (1 << (FLOAT_INT_BITS - 1))

/**
 * @brief Writes part of a packetentities message, including the entity number.
 * Can delta from either a baseline or a previous packet_entity
 * If to is NULL, a remove entity update will be sent
 * If force is not set, then nothing at all will be generated if the entity is
 * identical, under the assumption that the in-order delta code will catch it.
 * @param[out] msg
 * @param[in] from
 * @param[in] to
 * @param[in] force
 */
void MSG_WriteDeltaEntity(msg_t *msg, entityState_t *from, entityState_t *to, qboolean force)
{
	int        i, lc;
	int        numFields = sizeof(entityStateFields) / sizeof(entityStateFields[0]);
	netField_t *field;
	int        trunc;
	float      fullFloat;
	int        *fromF, *toF;

	// all fields should be 32 bits to avoid any compiler packing issues
	// the "number" field is not part of the field list
	// if this assert fails, someone added a field to the entityState_t
	// struct without updating the message fields
	etl_assert(numFields + 1 == sizeof(*from) / 4);

	// a NULL to is a delta remove message
	if (to == NULL)
	{
		if (from == NULL)
		{
			return;
		}
		if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -1))
		{
			Com_Printf("W|%3i: #%-3i remove\n", msg->cursize, from->number);
		}
		MSG_WriteBits(msg, from->number, GENTITYNUM_BITS);
		MSG_WriteBits(msg, 1, 1);
		return;
	}

	if (to->number < 0 || to->number >= MAX_GENTITIES)
	{
		Com_Error(ERR_FATAL, "MSG_WriteDeltaEntity: Bad entity number: %i", to->number);
	}

	lc = 0;
	// build the change vector as bytes so it is endien independent
	for (i = 0, field = entityStateFields ; i < numFields ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);
		if (*fromF != *toF)
		{
			lc = i + 1;

			field->used++;
		}
	}

	if (lc == 0)
	{
		// nothing at all changed
		if (!force)
		{
			return;     // nothing at all
		}
		// write two bits for no change
		MSG_WriteBits(msg, to->number, GENTITYNUM_BITS);
		MSG_WriteBits(msg, 0, 1);       // not removed
		MSG_WriteBits(msg, 0, 1);       // no delta
		return;
	}

	MSG_WriteBits(msg, to->number, GENTITYNUM_BITS);
	MSG_WriteBits(msg, 0, 1);           // not removed
	MSG_WriteBits(msg, 1, 1);           // we have a delta

	MSG_WriteByte(msg, lc);     // # of changes

	oldsize += numFields;

	//Com_Printf( "Delta for ent %i: ", to->number );

	for (i = 0, field = entityStateFields ; i < lc ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);

		if (*fromF == *toF)
		{
			MSG_WriteBits(msg, 0, 1);   // no change

			wastedbits++;

			continue;
		}

		MSG_WriteBits(msg, 1, 1);   // changed

		if (field->bits == 0)
		{
			// float
			fullFloat = *(float *)toF;
			trunc     = (int)fullFloat;

			if (fullFloat == 0.0f)
			{
				MSG_WriteBits(msg, 0, 1);
				oldsize += FLOAT_INT_BITS;
			}
			else
			{
				MSG_WriteBits(msg, 1, 1);
				if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
				    trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS))
				{
					// send as small integer
					MSG_WriteBits(msg, 0, 1);
					MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
					//if (print) {
					//  Com_Printf( "%s:%i ", field->name, trunc );
					//}
				}
				else
				{
					// send as full floating point value
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteBits(msg, *toF, 32);
					//if (print) {
					//  Com_Printf( "%s:%f ", field->name, *(float *)toF );
					//}
				}
			}
		}
		else
		{
			if (*toF == 0)
			{
				MSG_WriteBits(msg, 0, 1);
			}
			else
			{
				MSG_WriteBits(msg, 1, 1);
				// integer
				MSG_WriteBits(msg, *toF, field->bits);
				//if ( print ) {
				//  Com_Printf( "%s:%i ", field->name, *toF );
				//}
			}
		}
	}

	//Com_Printf( "\n" );

	/*
	    c = msg->cursize - c;

	    if ( print ) {
	        if ( msg->bit == 0 ) {
	            endBit = msg->cursize * 8 - GENTITYNUM_BITS;
	        } else {
	            endBit = ( msg->cursize - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
	        }
	        Com_Printf( " (%i bits)\n", endBit - startBit  );
	    }
	*/
}

extern cvar_t *cl_shownet;

/**
 * @brief The entity number has already been read from the message, which
 * is how the from state is identified.
 *
 * If the delta removes the entity, entityState_t->number will be set to MAX_GENTITIES-1
 *
 * Can go from either a baseline or a previous packet_entity
 * @param[in] msg
 * @param[in] from
 * @param[out] to
 * @param[in] number
 */
void MSG_ReadDeltaEntity(msg_t *msg, entityState_t *from, entityState_t *to,
                         int number)
{
	int        i, lc;
	int        numFields;
	netField_t *field;
	int        *fromF, *toF;
	int        print;
	int        trunc;
	int        startBit, endBit;

	if (number < 0 || number >= MAX_GENTITIES)
	{
		Com_Error(ERR_DROP, "MSG_ReadDeltaEntity: Bad delta entity number: %i", number);
	}

	if (msg->bit == 0)
	{
		startBit = msg->readcount * 8 - GENTITYNUM_BITS;
	}
	else
	{
		startBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
	}

	// check for a remove
	if (MSG_ReadBits(msg, 1) == 1)
	{
		Com_Memset(to, 0, sizeof(*to));
		to->number = MAX_GENTITIES - 1;
		if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -1))
		{
			Com_Printf("%3i: #%-3i remove\n", msg->readcount, number);
		}
		return;
	}

	// check for no delta
	if (MSG_ReadBits(msg, 1) == 0)
	{
		*to        = *from;
		to->number = number;
		return;
	}

	numFields = sizeof(entityStateFields) / sizeof(entityStateFields[0]);
	lc        = MSG_ReadByte(msg);

	if (lc > numFields || lc < 0)
	{
		Com_Error(ERR_DROP, "invalid entityState field count");
	}

	// shownet 2/3 will interleave with other printed info, -1 will
	// just print the delta records`
	if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -1))
	{
		print = 1;
		Com_Printf("%3i: #%-3i ", msg->readcount, to->number);
	}
	else
	{
		print = 0;
	}

	to->number = number;

	for (i = 0, field = entityStateFields ; i < lc ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);

		if (!MSG_ReadBits(msg, 1))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if (field->bits == 0)
			{
				// float
				if (MSG_ReadBits(msg, 1) == 0)
				{
					*(float *)toF = 0.0f;
				}
				else
				{
					if (MSG_ReadBits(msg, 1) == 0)
					{
						// integral float
						trunc = MSG_ReadBits(msg, FLOAT_INT_BITS);
						// bias to allow equal parts positive and negative
						trunc        -= FLOAT_INT_BIAS;
						*(float *)toF = trunc;
						if (print)
						{
							Com_Printf("%s:%i ", field->name, trunc);
						}
					}
					else
					{
						// full floating point value
						*toF = MSG_ReadBits(msg, 32);
						if (print)
						{
							Com_Printf("%s:%f ", field->name, *(float *)toF);
						}
					}
				}
			}
			else
			{
				if (MSG_ReadBits(msg, 1) == 0)
				{
					*toF = 0;
				}
				else
				{
					// integer
					*toF = MSG_ReadBits(msg, field->bits);
					if (print)
					{
						Com_Printf("%s:%i ", field->name, *toF);
					}
				}
			}
			//pcount[i]++;
		}
	}
	for (i = lc, field = &entityStateFields[lc] ; i < numFields ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);
		// no change
		*toF = *fromF;
	}

	if (print)
	{
		if (msg->bit == 0)
		{
			endBit = msg->readcount * 8 - GENTITYNUM_BITS;
		}
		else
		{
			endBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf(" (%i bits)\n", endBit - startBit);
	}
}

/*
============================================================================

entityShared_t communication

============================================================================
*/

/*
 * Return (v ? floor(log2(v)) : 0) when 0 <= v < 1<<[8, 16, 32, 64].
 * Inefficient algorithm, intended for compile-time constants.
 * Courtesy of Hallvard B Furuseth
 */
#define LOG2_8BIT(v)  (8 - 90 / (((v) / 4 + 14) | 1) - 2 / ((v) / 2 + 1))
#define LOG2_16BIT(v) (8 * ((v) > 255) + LOG2_8BIT((v) >> 8 * ((v) > 255)))
#define LOG2_32BIT(v) \
		(16 * ((v) > 65535L) + LOG2_16BIT((v) * 1L >> 16 * ((v) > 65535L)))
#define LOG2_64BIT(v) \
		(32 * ((v) / 2L >> 31 > 0) \
		 + LOG2_32BIT((v) * 1L >> 16 * ((v) / 2L >> 31 > 0) \
					  >> 16 * ((v) / 2L >> 31 > 0)))

/// Compute the number of clients bits at compile-time (this is necessary else the compiler will throw an error because this is not a constant)
#define CLIENTNUM_BITS  LOG2_8BIT(MAX_CLIENTS)

/// Using the stringizing operator to save typing...
#define ESF(x) # x, (size_t)&((entityShared_t *)0)->x

netField_t entitySharedFields[] =
{
	{ ESF(linked),           1,               0 },
	{ ESF(linkcount),        8,               0 }, ///< enough to see whether the linkcount has changed
	                                               ///< (assuming it doesn't change 256 times in 1 frame)
	{ ESF(bmodel),           1,               0 },
	{ ESF(svFlags),          12,              0 },
	{ ESF(singleClient),     CLIENTNUM_BITS,  0 },
	{ ESF(contents),         32,              0 },
	{ ESF(ownerNum),         GENTITYNUM_BITS, 0 },
	{ ESF(mins[0]),          0,               0 },
	{ ESF(mins[1]),          0,               0 },
	{ ESF(mins[2]),          0,               0 },
	{ ESF(maxs[0]),          0,               0 },
	{ ESF(maxs[1]),          0,               0 },
	{ ESF(maxs[2]),          0,               0 },
	{ ESF(absmin[0]),        0,               0 },
	{ ESF(absmin[1]),        0,               0 },
	{ ESF(absmin[2]),        0,               0 },
	{ ESF(absmax[0]),        0,               0 },
	{ ESF(absmax[1]),        0,               0 },
	{ ESF(absmax[2]),        0,               0 },
	{ ESF(currentOrigin[0]), 0,               0 },
	{ ESF(currentOrigin[1]), 0,               0 },
	{ ESF(currentOrigin[2]), 0,               0 },
	{ ESF(currentAngles[0]), 0,               0 },
	{ ESF(currentAngles[1]), 0,               0 },
	{ ESF(currentAngles[2]), 0,               0 },
	{ ESF(ownerNum),         32,              0 },
	{ ESF(eventTime),        32,              0 },
	{ ESF(worldflags),       32,              0 },
	{ ESF(snapshotCallback), 1,               0 }
};

netField_t ettventitySharedFields[] =
{
	{ ESF(currentOrigin[0]), 0,  0 },
	{ ESF(currentOrigin[1]), 0,  0 },
	{ ESF(currentOrigin[2]), 0,  0 },
	{ ESF(currentAngles[0]), 0,  0 },
	{ ESF(currentAngles[1]), 0,  0 },
	{ ESF(currentAngles[2]), 0,  0 },
	{ ESF(svFlags),          32, 0 },
	{ ESF(mins[0]),          0,  0 },
	{ ESF(mins[1]),          0,  0 },
	{ ESF(mins[2]),          0,  0 },
	{ ESF(maxs[0]),          0,  0 },
	{ ESF(maxs[1]),          0,  0 },
	{ ESF(maxs[2]),          0,  0 },
	{ ESF(singleClient),     8,  0 }
};

/**
* @brief MSG_ETTV_WriteDeltaSharedEntity
* @details Appends part of a packetentities message with entityShared_t, without the entity number.
*          Can delta from either a baseline or a previous packet_entity.
* @param[out] msg
* @param[in] from
* @param[in] to
* @param[in] force
*/
void MSG_ETTV_WriteDeltaSharedEntity(msg_t *msg, void *from, void *to, qboolean force)
{
	int        i, lc;
	int        numFields;
	netField_t *field;
	int        trunc;
	float      fullFloat;
	int        *fromF, *toF;

	numFields = ARRAY_LEN(ettventitySharedFields);

	// write magic byte
	MSG_WriteBits(msg, 0x77, 8);

	// a NULL to is a delta remove message
	if (to == NULL)
	{
		if (from == NULL)
		{
			return;
		}
		MSG_WriteBits(msg, 1, 1);
		return;
	}

	// all fields should be 32 bits to avoid any compiler packing issues
	// if this assert fails, someone added a field to the entityShared_t
	// struct without updating the message fields
	//etl_assert(numFields == (sizeof(entityShared_t) - sizeof(entityState_t)) / 4);

	lc = 0;
	// build the change vector as bytes so it is endien independent
	for (i = 0, field = ettventitySharedFields; i < numFields; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF   = (int *)((byte *)to + field->offset);

		if (*fromF != *toF)
		{
			lc = i + 1;
		}
	}

	if (lc == 0)
	{
		// nothing at all changed
		if (!force)
		{
			return;     // nothing at all
		}
		// write a bits for no change
		//MSG_WriteBits(msg, number, GENTITYNUM_BITS);
		MSG_WriteBits(msg, 0, 1);
		MSG_WriteBits(msg, 0, 1);       // no delta
		return;
	}

	//MSG_WriteBits(msg, number, GENTITYNUM_BITS);
	MSG_WriteBits(msg, 0, 1);
	MSG_WriteBits(msg, 1, 1);           // we have a delta

	MSG_WriteByte(msg, lc);     // # of changes

	oldsize += numFields;

	for (i = 0, field = ettventitySharedFields; i < lc; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF   = (int *)((byte *)to + field->offset);

		if (*fromF == *toF)
		{
			MSG_WriteBits(msg, 0, 1);   // no change
			continue;
		}

		MSG_WriteBits(msg, 1, 1);   // changed

		if (field->bits == 0)
		{
			// float
			fullFloat = *(float *)toF;
			trunc     = (int)fullFloat;

			if (fullFloat == 0.0f)
			{
				MSG_WriteBits(msg, 0, 1);
				oldsize += FLOAT_INT_BITS;
			}
			else
			{
				MSG_WriteBits(msg, 1, 1);
				if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
				    trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS))
				{
					// send as small integer
					MSG_WriteBits(msg, 0, 1);
					MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
				}
				else
				{
					// send as full floating point value
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteBits(msg, *toF, 32);
				}
			}
		}
		else
		{
			if (*toF == 0)
			{
				MSG_WriteBits(msg, 0, 1);
			}
			else
			{
				MSG_WriteBits(msg, 1, 1);
				// integer
				MSG_WriteBits(msg, *toF, field->bits);
			}
		}
	}
}

/**
* @brief MSG_ETTV_ReadDeltaSharedEntity unused
* @param[in] msg
* @param[in] from
* @param[in] to
*/
/*
void MSG_ETTV_ReadDeltaSharedEntity(msg_t *msg, void *from, void *to)
{
	int        i, lc;
	int        numFields;
	netField_t *field;
	int        *fromF, *toF;
	int        trunc;
	int        magic;

	// read magic byte
	magic = MSG_ReadBits(msg, 8);
	if (magic != 0x77)
	{
		Com_Error(ERR_DROP, "MSG_ETTV_ReadDeltaSharedEntity: wrong magic byte 0x%x", magic);
	}

	// check for a remove
	if (MSG_ReadBits(msg, 1) == 1)
	{
		Com_Memset(to, 0, sizeof(*to));
		return;
	}

	// check for no delta
	if (MSG_ReadBits(msg, 1) == 0)
	{
		*(entityShared_t *)to = *(entityShared_t *)from;
		return;
	}

	numFields = sizeof(ettventitySharedFields) / sizeof(ettventitySharedFields[0]);
	lc = MSG_ReadByte(msg);

	if (lc > numFields || lc < 0)
	{
		Com_Error(ERR_DROP, "MSG_ETTV_ReadDeltaSharedEntity: invalid entityShared field count");
	}

	for (i = 0, field = ettventitySharedFields; i < lc; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF = (int *)((byte *)to + field->offset);

		if (!MSG_ReadBits(msg, 1))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if (field->bits == 0)
			{
				// float
				if (MSG_ReadBits(msg, 1) == 0)
				{
					*(float *)toF = 0.0f;
				}
				else
				{
					if (MSG_ReadBits(msg, 1) == 0)
					{
						// integral float
						trunc = MSG_ReadBits(msg, FLOAT_INT_BITS);
						// bias to allow equal parts positive and negative
						trunc -= FLOAT_INT_BIAS;
						*(float *)toF = trunc;
					}
					else
					{
						// full floating point value
						*toF = MSG_ReadBits(msg, 32);
					}
				}
			}
			else
			{
				if (MSG_ReadBits(msg, 1) == 0)
				{
					*toF = 0;
				}
				else
				{
					// integer
					*toF = MSG_ReadBits(msg, field->bits);
				}
			}
			//			pcount[i]++;
		}
	}
	for (i = lc, field = &ettventitySharedFields[lc]; i < numFields; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF = (int *)((byte *)to + field->offset);
		// no change
		*toF = *fromF;
	}
}*/

/**
 * @brief MSG_WriteDeltaSharedEntity
 * @param[out] msg
 * @param[in] from
 * @param[in] to
 * @param[in] force
 * @param[in] number
 */
void MSG_WriteDeltaSharedEntity(msg_t *msg, void *from, void *to, qboolean force, int number)
{
	int        i, lc;
	int        numFields;
	netField_t *field;
	int        trunc;
	float      fullFloat;
	int        *fromF, *toF;

	numFields = ARRAY_LEN(entitySharedFields);

	// all fields should be 32 bits to avoid any compiler packing issues
	// if this assert fails, someone added a field to the entityShared_t
	// struct without updating the message fields
	//etl_assert(numFields == (sizeof(entityShared_t) - sizeof(entityState_t)) / 4);

	lc = 0;
	// build the change vector as bytes so it is endien independent
	for (i = 0, field = entitySharedFields ; i < numFields ; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF   = (int *)((byte *)to + field->offset);

		if (*fromF != *toF)
		{
			lc = i + 1;
		}
	}

	if (lc == 0)
	{
		// nothing at all changed
		if (!force)
		{
			return;     // nothing at all
		}
		// write a bits for no change
		MSG_WriteBits(msg, number, GENTITYNUM_BITS);
		MSG_WriteBits(msg, 0, 1);       // no delta
		return;
	}

	MSG_WriteBits(msg, number, GENTITYNUM_BITS);
	MSG_WriteBits(msg, 1, 1);           // we have a delta

	MSG_WriteByte(msg, lc);     // # of changes

	oldsize += numFields;

	for (i = 0, field = entitySharedFields ; i < lc ; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF   = (int *)((byte *)to + field->offset);

		if (*fromF == *toF)
		{
			MSG_WriteBits(msg, 0, 1);   // no change
			continue;
		}

		MSG_WriteBits(msg, 1, 1);   // changed

		if (field->bits == 0)
		{
			// float
			fullFloat = *(float *)toF;
			trunc     = (int)fullFloat;

			if (fullFloat == 0.0f)
			{
				MSG_WriteBits(msg, 0, 1);
				oldsize += FLOAT_INT_BITS;
			}
			else
			{
				MSG_WriteBits(msg, 1, 1);
				if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
				    trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS))
				{
					// send as small integer
					MSG_WriteBits(msg, 0, 1);
					MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
				}
				else
				{
					// send as full floating point value
					MSG_WriteBits(msg, 1, 1);
					MSG_WriteBits(msg, *toF, 32);
				}
			}
		}
		else
		{
			if (*toF == 0)
			{
				MSG_WriteBits(msg, 0, 1);
			}
			else
			{
				MSG_WriteBits(msg, 1, 1);
				// integer
				MSG_WriteBits(msg, *toF, field->bits);
			}
		}
	}
}

/**
 * @brief MSG_ReadDeltaSharedEntity
 * @param[in] msg
 * @param[in] from
 * @param[in] to
 * @param number - unused
 */
void MSG_ReadDeltaSharedEntity(msg_t *msg, void *from, void *to, int number)
{
	int        i, lc;
	int        numFields;
	netField_t *field;
	int        *fromF, *toF;
	int        trunc;

	// check for no delta
	if (MSG_ReadBits(msg, 1) == 0)
	{
		*(entityShared_t *)to = *(entityShared_t *)from;
		return;
	}

	numFields = sizeof(entitySharedFields) / sizeof(entitySharedFields[0]);
	lc        = MSG_ReadByte(msg);

	if (lc > numFields || lc < 0)
	{
		Com_Error(ERR_DROP, "invalid entityShared field count");
	}

	for (i = 0, field = entitySharedFields ; i < lc ; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF   = (int *)((byte *)to + field->offset);

		if (!MSG_ReadBits(msg, 1))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if (field->bits == 0)
			{
				// float
				if (MSG_ReadBits(msg, 1) == 0)
				{
					*(float *)toF = 0.0f;
				}
				else
				{
					if (MSG_ReadBits(msg, 1) == 0)
					{
						// integral float
						trunc = MSG_ReadBits(msg, FLOAT_INT_BITS);
						// bias to allow equal parts positive and negative
						trunc        -= FLOAT_INT_BIAS;
						*(float *)toF = trunc;
					}
					else
					{
						// full floating point value
						*toF = MSG_ReadBits(msg, 32);
					}
				}
			}
			else
			{
				if (MSG_ReadBits(msg, 1) == 0)
				{
					*toF = 0;
				}
				else
				{
					// integer
					*toF = MSG_ReadBits(msg, field->bits);
				}
			}
//			pcount[i]++;
		}
	}
	for (i = lc, field = &entitySharedFields[lc] ; i < numFields ; i++, field++)
	{
		fromF = (int *)((byte *)from + field->offset);
		toF   = (int *)((byte *)to + field->offset);
		// no change
		*toF = *fromF;
	}
}

/*
============================================================================
player_state_t communication
============================================================================
*/

/// Using the stringizing operator to save typing...
#define PSF(x) # x, (size_t)&((playerState_t *)0)->x

netField_t playerStateFields[] =
{
	{ PSF(commandTime),          32,              0 },
	{ PSF(pm_type),              8,               0 },
	{ PSF(bobCycle),             8,               0 },
	{ PSF(pm_flags),             16,              0 },
	{ PSF(pm_time),              -16,             0 },
	{ PSF(origin[0]),            0,               0 },
	{ PSF(origin[1]),            0,               0 },
	{ PSF(origin[2]),            0,               0 },
	{ PSF(velocity[0]),          0,               0 },
	{ PSF(velocity[1]),          0,               0 },
	{ PSF(velocity[2]),          0,               0 },
	{ PSF(weaponTime),           -16,             0 },
	{ PSF(weaponDelay),          -16,             0 },
	{ PSF(grenadeTimeLeft),      -16,             0 },
	{ PSF(gravity),              16,              0 },
	{ PSF(leanf),                0,               0 },
	{ PSF(speed),                16,              0 },
	{ PSF(delta_angles[0]),      16,              0 },
	{ PSF(delta_angles[1]),      16,              0 },
	{ PSF(delta_angles[2]),      16,              0 },
	{ PSF(groundEntityNum),      GENTITYNUM_BITS, 0 },
	{ PSF(legsTimer),            16,              0 },
	{ PSF(torsoTimer),           16,              0 },
	{ PSF(legsAnim),             ANIM_BITS,       0 },
	{ PSF(torsoAnim),            ANIM_BITS,       0 },
	{ PSF(movementDir),          8,               0 },
	{ PSF(eFlags),               24,              0 },
	{ PSF(eventSequence),        8,               0 },
	{ PSF(events[0]),            8,               0 },
	{ PSF(events[1]),            8,               0 },
	{ PSF(events[2]),            8,               0 },
	{ PSF(events[3]),            8,               0 },
	{ PSF(eventParms[0]),        8,               0 },
	{ PSF(eventParms[1]),        8,               0 },
	{ PSF(eventParms[2]),        8,               0 },
	{ PSF(eventParms[3]),        8,               0 },
	{ PSF(clientNum),            8,               0 },
	{ PSF(weapons[0]),           32,              0 },
	{ PSF(weapons[1]),           32,              0 },
	{ PSF(weapon),               7,               0 },
	{ PSF(weaponstate),          4,               0 },
	{ PSF(weapAnim),             10,              0 },
	{ PSF(viewangles[0]),        0,               0 },
	{ PSF(viewangles[1]),        0,               0 },
	{ PSF(viewangles[2]),        0,               0 },
	{ PSF(viewheight),           -8,              0 },
	{ PSF(damageEvent),          8,               0 },
	{ PSF(damageYaw),            8,               0 },
	{ PSF(damagePitch),          8,               0 },
	{ PSF(damageCount),          8,               0 },
	{ PSF(mins[0]),              0,               0 },
	{ PSF(mins[1]),              0,               0 },
	{ PSF(mins[2]),              0,               0 },
	{ PSF(maxs[0]),              0,               0 },
	{ PSF(maxs[1]),              0,               0 },
	{ PSF(maxs[2]),              0,               0 },
	{ PSF(crouchMaxZ),           0,               0 },
	{ PSF(crouchViewHeight),     0,               0 },
	{ PSF(standViewHeight),      0,               0 },
	{ PSF(deadViewHeight),       0,               0 },
	{ PSF(runSpeedScale),        0,               0 },
	{ PSF(sprintSpeedScale),     0,               0 },
	{ PSF(crouchSpeedScale),     0,               0 },
	{ PSF(friction),             0,               0 },
	{ PSF(viewlocked),           8,               0 },
	{ PSF(viewlocked_entNum),    16,              0 },
	{ PSF(nextWeapon),           8,               0 },
	{ PSF(teamNum),              8,               0 },
	{ PSF(onFireStart),          32,              0 },
	{ PSF(curWeapHeat),          8,               0 },
	{ PSF(aimSpreadScale),       8,               0 },
	{ PSF(serverCursorHint),     8,               0 },
	{ PSF(serverCursorHintVal),  8,               0 },
	{ PSF(classWeaponTime),      32,              0 },
	{ PSF(identifyClient),       8,               0 },
	{ PSF(identifyClientHealth), 8,               0 },
	{ PSF(aiState),              2,               0 },
};

/**
 * @brief qsort_playerstatefields
 * @param[in] a
 * @param[in] b
 * @return
 */
static int QDECL qsort_playerstatefields(const void *a, const void *b)
{
	const int aa = *((const int *)a);
	const int bb = *((const int *)b);

	if (playerStateFields[aa].used > playerStateFields[bb].used)
	{
		return -1;
	}
	if (playerStateFields[bb].used > playerStateFields[aa].used)
	{
		return 1;
	}
	return 0;
}

/**
 * @brief MSG_PrioritisePlayerStateFields
 */
void MSG_PrioritisePlayerStateFields(void)
{
	int fieldorders[sizeof(playerStateFields) / sizeof(playerStateFields[0])];
	int numfields = sizeof(playerStateFields) / sizeof(playerStateFields[0]);
	int i;

	for (i = 0; i < numfields; i++)
	{
		fieldorders[i] = i;
	}

	qsort(fieldorders, numfields, sizeof(int), qsort_playerstatefields);

	Com_Printf("Playerstate fields in order of priority\n");
	Com_Printf("netField_t playerStateFields[] = {\n");
	for (i = 0; i < numfields; i++)
	{
		Com_Printf("{ PSF(%s), %i },\n", playerStateFields[fieldorders[i]].name, playerStateFields[fieldorders[i]].bits);
	}
	Com_Printf("};\n");
}

/**
 * @brief MSG_WriteDeltaPlayerstate
 * @param[out] msg
 * @param[in] from
 * @param[in] to
 */
void MSG_WriteDeltaPlayerstate(msg_t *msg, struct playerState_s *from, struct playerState_s *to)
{
	int           i, j, lc;
	playerState_t dummy;
	int           statsbits;
	int           persistantbits;
	int           ammobits[4];
	int           clipbits;
	int           powerupbits;
	int           holdablebits;
	int           numFields;
	netField_t    *field;
	int           *fromF, *toF;
	float         fullFloat;
	int           trunc;
	int           startBit, endBit;
	int           print;

	if (!from)
	{
		from = &dummy;
		Com_Memset(&dummy, 0, sizeof(dummy));
	}

	if (msg->bit == 0)
	{
		startBit = msg->cursize * 8 - GENTITYNUM_BITS;
	}
	else
	{
		startBit = (msg->cursize - 1) * 8 + msg->bit - GENTITYNUM_BITS;
	}

	// shownet 2/3 will interleave with other printed info, -2 will
	// just print the delta records
	if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -2))
	{
		print = 1;
		Com_Printf("W|%3i: playerstate ", msg->cursize);
	}
	else
	{
		print = 0;
	}

	numFields = sizeof(playerStateFields) / sizeof(playerStateFields[0]);

	lc = 0;
	for (i = 0, field = playerStateFields ; i < numFields ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);
		if (*fromF != *toF)
		{
			lc = i + 1;

			field->used++;
		}
	}

	MSG_WriteByte(msg, lc);     // # of changes

	oldsize += numFields - lc;

	for (i = 0, field = playerStateFields ; i < lc ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);

		if (*fromF == *toF)
		{
			wastedbits++;

			MSG_WriteBits(msg, 0, 1);   // no change
			continue;
		}

		MSG_WriteBits(msg, 1, 1);   // changed
		//pcount[i]++;

		if (field->bits == 0)
		{
			// float
			fullFloat = *(float *)toF;
			trunc     = (int)fullFloat;

			if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 &&
			    trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS))
			{
				// send as small integer
				MSG_WriteBits(msg, 0, 1);
				MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
				//if ( print ) {
				//  Com_Printf( "%s:%i ", field->name, trunc );
				//}
			}
			else
			{
				// send as full floating point value
				MSG_WriteBits(msg, 1, 1);
				MSG_WriteBits(msg, *toF, 32);
				//if ( print ) {
				//  Com_Printf( "%s:%f ", field->name, *(float *)toF );
				//}
			}
		}
		else
		{
			// integer
			MSG_WriteBits(msg, *toF, field->bits);
			//if ( print ) {
			//  Com_Printf( "%s:%i ", field->name, *toF );
			//}
		}
	}

	//
	// send the arrays
	//
	statsbits = 0;
	for (i = 0 ; i < MAX_STATS ; i++)
	{
		if (to->stats[i] != from->stats[i])
		{
			statsbits |= 1 << i;
		}
	}
	persistantbits = 0;
	for (i = 0 ; i < MAX_PERSISTANT ; i++)
	{
		if (to->persistant[i] != from->persistant[i])
		{
			persistantbits |= 1 << i;
		}
	}
	holdablebits = 0;
	for (i = 0 ; i < MAX_HOLDABLE ; i++)
	{
		if (to->holdable[i] != from->holdable[i])
		{
			holdablebits |= 1 << i;
		}
	}
	powerupbits = 0;
	for (i = 0 ; i < MAX_POWERUPS ; i++)
	{
		if (to->powerups[i] != from->powerups[i])
		{
			powerupbits |= 1 << i;
		}
	}

	if (statsbits || persistantbits || holdablebits || powerupbits)
	{
		MSG_WriteBits(msg, 1, 1);   // something changed

		if (statsbits)
		{
			MSG_WriteBits(msg, 1, 1);   // changed
			MSG_WriteShort(msg, statsbits);
			for (i = 0 ; i < MAX_STATS ; i++)
			{
				if (statsbits & (1 << i))
				{
					MSG_WriteShort(msg, to->stats[i]);
				}
			}
		}
		else
		{
			MSG_WriteBits(msg, 0, 1);   // no change to stats
		}

		if (persistantbits)
		{
			MSG_WriteBits(msg, 1, 1);   // changed
			MSG_WriteShort(msg, persistantbits);
			for (i = 0 ; i < MAX_PERSISTANT ; i++)
			{
				if (persistantbits & (1 << i))
				{
					MSG_WriteShort(msg, to->persistant[i]);
				}
			}
		}
		else
		{
			MSG_WriteBits(msg, 0, 1);   // no change to persistant
		}

		if (holdablebits)
		{
			MSG_WriteBits(msg, 1, 1);   // changed
			MSG_WriteShort(msg, holdablebits);
			for (i = 0 ; i < MAX_HOLDABLE ; i++)
			{
				if (holdablebits & (1 << i))
				{
					MSG_WriteShort(msg, to->holdable[i]);
				}
			}
		}
		else
		{
			MSG_WriteBits(msg, 0, 1);   // no change to holdables
		}

		if (powerupbits)
		{
			MSG_WriteBits(msg, 1, 1);   // changed
			MSG_WriteShort(msg, powerupbits);
			for (i = 0 ; i < MAX_POWERUPS ; i++)
			{
				if (powerupbits & (1 << i))
				{
					MSG_WriteLong(msg, to->powerups[i]);
				}
			}
		}
		else
		{
			MSG_WriteBits(msg, 0, 1);   // no change to powerups
		}
	}
	else
	{
		MSG_WriteBits(msg, 0, 1);   // no change to any
		oldsize += 4;
	}

	// Split this into two groups using shorts so it wouldn't have
	// to use a long every time ammo changed for any weap.
	// this seemed like a much friendlier option than making it
	// read/write a long for any ammo change.

	// j == 0 : weaps 0-15
	// j == 1 : weaps 16-31
	// j == 2 : weaps 32-47 // now up to 64 (but still pretty net-friendly)
	// j == 3 : weaps 48-63

	// ammo stored
	for (j = 0; j < 4; j++)      // modified for 64 weaps
	{
		ammobits[j] = 0;
		for (i = 0 ; i < 16 ; i++)
		{
			if (to->ammo[i + (j * 16)] != from->ammo[i + (j * 16)])
			{
				ammobits[j] |= 1 << i;
			}
		}
	}

	// also encapsulated ammo changes into one check. Clip values will change frequently,
	// but ammo will not.  (only when you get ammo/reload rather than each shot)
	if (ammobits[0] || ammobits[1] || ammobits[2] || ammobits[3])      // if any were set...
	{
		MSG_WriteBits(msg, 1, 1);   // changed
		for (j = 0; j < 4; j++)
		{
			if (ammobits[j])
			{
				MSG_WriteBits(msg, 1, 1);   // changed
				MSG_WriteShort(msg, ammobits[j]);
				for (i = 0 ; i < 16 ; i++)
				{
					if (ammobits[j] & (1 << i))
					{
						MSG_WriteShort(msg, to->ammo[i + (j * 16)]);
					}
				}
			}
			else
			{
				MSG_WriteBits(msg, 0, 1);   // no change
			}
		}
	}
	else
	{
		MSG_WriteBits(msg, 0, 1);   // no change
	}

	// ammo in clip
	for (j = 0; j < 4; j++)      // modified for 64 weaps
	{
		clipbits = 0;
		for (i = 0 ; i < 16 ; i++)
		{
			if (to->ammoclip[i + (j * 16)] != from->ammoclip[i + (j * 16)])
			{
				clipbits |= 1 << i;
			}
		}
		if (clipbits)
		{
			MSG_WriteBits(msg, 1, 1);   // changed
			MSG_WriteShort(msg, clipbits);
			for (i = 0 ; i < 16 ; i++)
			{
				if (clipbits & (1 << i))
				{
					MSG_WriteShort(msg, to->ammoclip[i + (j * 16)]);
				}
			}
		}
		else
		{
			MSG_WriteBits(msg, 0, 1);   // no change
		}
	}

	if (print)
	{
		if (msg->bit == 0)
		{
			endBit = msg->cursize * 8 - GENTITYNUM_BITS;
		}
		else
		{
			endBit = (msg->cursize - 1) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf(" (%i bits)\n", endBit - startBit);
	}
}

/**
 * @brief MSG_ReadDeltaPlayerstate
 * @param[in] msg
 * @param[in] from
 * @param[out] to
 */
void MSG_ReadDeltaPlayerstate(msg_t *msg, playerState_t *from, playerState_t *to)
{
	int           i, j, lc;
	int           bits;
	netField_t    *field;
	int           numFields;
	int           startBit, endBit;
	int           print;
	int           *fromF, *toF;
	int           trunc;
	playerState_t dummy;

	if (!from)
	{
		from = &dummy;
		Com_Memset(&dummy, 0, sizeof(dummy));
	}
	*to = *from;

	if (msg->bit == 0)
	{
		startBit = msg->readcount * 8 - GENTITYNUM_BITS;
	}
	else
	{
		startBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
	}

	// shownet 2/3 will interleave with other printed info, -2 will
	// just print the delta records
	if (cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -2))
	{
		print = 1;
		Com_Printf("%3i: playerstate ", msg->readcount);
	}
	else
	{
		print = 0;
	}

	numFields = sizeof(playerStateFields) / sizeof(playerStateFields[0]);
	lc        = MSG_ReadByte(msg);

	if (lc > numFields || lc < 0)
	{
		Com_Error(ERR_DROP, "invalid playerState field count");
	}

	for (i = 0, field = playerStateFields ; i < lc ; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);

		if (!MSG_ReadBits(msg, 1))
		{
			// no change
			*toF = *fromF;
		}
		else
		{
			if (field->bits == 0)
			{
				// float
				if (MSG_ReadBits(msg, 1) == 0)
				{
					// integral float
					trunc = MSG_ReadBits(msg, FLOAT_INT_BITS);
					// bias to allow equal parts positive and negative
					trunc        -= FLOAT_INT_BIAS;
					*(float *)toF = trunc;
					if (print)
					{
						Com_Printf("%s:%i ", field->name, trunc);
					}
				}
				else
				{
					// full floating point value
					*toF = MSG_ReadBits(msg, 32);
					if (print)
					{
						Com_Printf("%s:%f ", field->name, *(float *)toF);
					}
				}
			}
			else
			{
				// integer
				*toF = MSG_ReadBits(msg, field->bits);
				if (print)
				{
					Com_Printf("%s:%i ", field->name, *toF);
				}
			}
		}
	}
	for (i = lc, field = &playerStateFields[lc]; i < numFields; i++, field++)
	{
		fromF = ( int * )((byte *)from + field->offset);
		toF   = ( int * )((byte *)to + field->offset);
		// no change
		*toF = *fromF;
	}

	// read the arrays
	if (MSG_ReadBits(msg, 1))        // one general bit tells if any of this infrequently changing stuff has changed
	{   // parse stats
		if (MSG_ReadBits(msg, 1))
		{
			LOG("PS_STATS");
			bits = MSG_ReadShort(msg);
			for (i = 0 ; i < MAX_STATS ; i++)
			{
				if (bits & (1 << i))
				{
					to->stats[i] = MSG_ReadShort(msg);
				}
			}
		}

		// parse persistant stats
		if (MSG_ReadBits(msg, 1))
		{
			LOG("PS_PERSISTANT");
			bits = MSG_ReadShort(msg);
			for (i = 0 ; i < MAX_PERSISTANT ; i++)
			{
				if (bits & (1 << i))
				{
					to->persistant[i] = MSG_ReadShort(msg);
				}
			}
		}

		// parse holdable stats
		if (MSG_ReadBits(msg, 1))
		{
			LOG("PS_HOLDABLE");
			bits = MSG_ReadShort(msg);
			for (i = 0 ; i < MAX_HOLDABLE ; i++)
			{
				if (bits & (1 << i))
				{
					to->holdable[i] = MSG_ReadShort(msg);
				}
			}
		}

		// parse powerups
		if (MSG_ReadBits(msg, 1))
		{
			LOG("PS_POWERUPS");
			bits = MSG_ReadShort(msg);
			for (i = 0 ; i < MAX_POWERUPS ; i++)
			{
				if (bits & (1 << i))
				{
					to->powerups[i] = MSG_ReadLong(msg);
				}
			}
		}
	}

	// Split this into two groups using shorts so it wouldn't have
	// to use a long every time ammo changed for any weap.
	// this seemed like a much friendlier option than making it
	// read/write a long for any ammo change.

	// parse ammo

	// j == 0 : weaps 0-15
	// j == 1 : weaps 16-31
	// j == 2 : weaps 32-47 // now up to 64 (but still pretty net-friendly)
	// j == 3 : weaps 48-63

	// ammo stored
	if (MSG_ReadBits(msg, 1))           // check for any ammo change (0-63)
	{
		for (j = 0; j < 4; j++)
		{
			if (MSG_ReadBits(msg, 1))
			{
				LOG("PS_AMMO");
				bits = MSG_ReadShort(msg);
				for (i = 0 ; i < 16 ; i++)
				{
					if (bits & (1 << i))
					{
						to->ammo[i + (j * 16)] = MSG_ReadShort(msg);
					}
				}
			}
		}
	}

	// ammo in clip
	for (j = 0; j < 4; j++)
	{
		if (MSG_ReadBits(msg, 1))
		{
			LOG("PS_AMMOCLIP");
			bits = MSG_ReadShort(msg);
			for (i = 0 ; i < 16 ; i++)
			{
				if (bits & (1 << i))
				{
					to->ammoclip[i + (j * 16)] = MSG_ReadShort(msg);
				}
			}
		}
	}

	if (print)
	{
		if (msg->bit == 0)
		{
			endBit = msg->readcount * 8 - GENTITYNUM_BITS;
		}
		else
		{
			endBit = (msg->readcount - 1) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf(" (%i bits)\n", endBit - startBit);
	}
}

/**
 * @var msg_hData
 * @brief Predefined set of nodes for Huffman compression
 */
int msg_hData[256] =
{
	250315,     // 0
	41193,      // 1
	6292,       // 2
	7106,       // 3
	3730,       // 4
	3750,       // 5
	6110,       // 6
	23283,      // 7
	33317,      // 8
	6950,       // 9
	7838,       // 10
	9714,       // 11
	9257,       // 12
	17259,      // 13
	3949,       // 14
	1778,       // 15
	8288,       // 16
	1604,       // 17
	1590,       // 18
	1663,       // 19
	1100,       // 20
	1213,       // 21
	1238,       // 22
	1134,       // 23
	1749,       // 24
	1059,       // 25
	1246,       // 26
	1149,       // 27
	1273,       // 28
	4486,       // 29
	2805,       // 30
	3472,       // 31
	21819,      // 32
	1159,       // 33
	1670,       // 34
	1066,       // 35
	1043,       // 36
	1012,       // 37
	1053,       // 38
	1070,       // 39
	1726,       // 40
	888,        // 41
	1180,       // 42
	850,        // 43
	960,        // 44
	780,        // 45
	1752,       // 46
	3296,       // 47
	10630,      // 48
	4514,       // 49
	5881,       // 50
	2685,       // 51
	4650,       // 52
	3837,       // 53
	2093,       // 54
	1867,       // 55
	2584,       // 56
	1949,       // 57
	1972,       // 58
	940,        // 59
	1134,       // 60
	1788,       // 61
	1670,       // 62
	1206,       // 63
	5719,       // 64
	6128,       // 65
	7222,       // 66
	6654,       // 67
	3710,       // 68
	3795,       // 69
	1492,       // 70
	1524,       // 71
	2215,       // 72
	1140,       // 73
	1355,       // 74
	971,        // 75
	2180,       // 76
	1248,       // 77
	1328,       // 78
	1195,       // 79
	1770,       // 80
	1078,       // 81
	1264,       // 82
	1266,       // 83
	1168,       // 84
	965,        // 85
	1155,       // 86
	1186,       // 87
	1347,       // 88
	1228,       // 89
	1529,       // 90
	1600,       // 91
	2617,       // 92
	2048,       // 93
	2546,       // 94
	3275,       // 95
	2410,       // 96
	3585,       // 97
	2504,       // 98
	2800,       // 99
	2675,       // 100
	6146,       // 101
	3663,       // 102
	2840,       // 103
	14253,      // 104
	3164,       // 105
	2221,       // 106
	1687,       // 107
	3208,       // 108
	2739,       // 109
	3512,       // 110
	4796,       // 111
	4091,       // 112
	3515,       // 113
	5288,       // 114
	4016,       // 115
	7937,       // 116
	6031,       // 117
	5360,       // 118
	3924,       // 119
	4892,       // 120
	3743,       // 121
	4566,       // 122
	4807,       // 123
	5852,       // 124
	6400,       // 125
	6225,       // 126
	8291,       // 127
	23243,      // 128
	7838,       // 129
	7073,       // 130
	8935,       // 131
	5437,       // 132
	4483,       // 133
	3641,       // 134
	5256,       // 135
	5312,       // 136
	5328,       // 137
	5370,       // 138
	3492,       // 139
	2458,       // 140
	1694,       // 141
	1821,       // 142
	2121,       // 143
	1916,       // 144
	1149,       // 145
	1516,       // 146
	1367,       // 147
	1236,       // 148
	1029,       // 149
	1258,       // 150
	1104,       // 151
	1245,       // 152
	1006,       // 153
	1149,       // 154
	1025,       // 155
	1241,       // 156
	952,        // 157
	1287,       // 158
	997,        // 159
	1713,       // 160
	1009,       // 161
	1187,       // 162
	879,        // 163
	1099,       // 164
	929,        // 165
	1078,       // 166
	951,        // 167
	1656,       // 168
	930,        // 169
	1153,       // 170
	1030,       // 171
	1262,       // 172
	1062,       // 173
	1214,       // 174
	1060,       // 175
	1621,       // 176
	930,        // 177
	1106,       // 178
	912,        // 179
	1034,       // 180
	892,        // 181
	1158,       // 182
	990,        // 183
	1175,       // 184
	850,        // 185
	1121,       // 186
	903,        // 187
	1087,       // 188
	920,        // 189
	1144,       // 190
	1056,       // 191
	3462,       // 192
	2240,       // 193
	4397,       // 194
	12136,      // 195
	7758,       // 196
	1345,       // 197
	1307,       // 198
	3278,       // 199
	1950,       // 200
	886,        // 201
	1023,       // 202
	1112,       // 203
	1077,       // 204
	1042,       // 205
	1061,       // 206
	1071,       // 207
	1484,       // 208
	1001,       // 209
	1096,       // 210
	915,        // 211
	1052,       // 212
	995,        // 213
	1070,       // 214
	876,        // 215
	1111,       // 216
	851,        // 217
	1059,       // 218
	805,        // 219
	1112,       // 220
	923,        // 221
	1103,       // 222
	817,        // 223
	1899,       // 224
	1872,       // 225
	976,        // 226
	841,        // 227
	1127,       // 228
	956,        // 229
	1159,       // 230
	950,        // 231
	7791,       // 232
	954,        // 233
	1289,       // 234
	933,        // 235
	1127,       // 236
	3207,       // 237
	1020,       // 238
	927,        // 239
	1355,       // 240
	768,        // 241
	1040,       // 242
	745,        // 243
	952,        // 244
	805,        // 245
	1073,       // 246
	740,        // 247
	1013,       // 248
	805,        // 249
	1008,       // 250
	796,        // 251
	996,        // 252
	1057,       // 253
	11457,      // 254
	13504,      // 255
};

/**
 * @brief MSG_initHuffman
 */
void MSG_initHuffman(void)
{
	int i, j;

	msgInit = qtrue;
	Huff_Init(&msgHuff);
	for (i = 0; i < 256; i++)
	{
		for (j = 0; j < msg_hData[i]; j++)
		{
			Huff_addRef(&msgHuff.compressor, (byte)i);    // Do update
			Huff_addRef(&msgHuff.decompressor, (byte)i);  // Do update
		}
	}
}
