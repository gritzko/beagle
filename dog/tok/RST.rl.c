
/* #line 1 "RST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "RST.h"

ok64 RSTonComment (u8cs tok, RSTstate* state);
ok64 RSTonString (u8cs tok, RSTstate* state);
ok64 RSTonNumber (u8cs tok, RSTstate* state);
ok64 RSTonAttr (u8cs tok, RSTstate* state);
ok64 RSTonWord (u8cs tok, RSTstate* state);
ok64 RSTonPunct (u8cs tok, RSTstate* state);
ok64 RSTonSpace (u8cs tok, RSTstate* state);


/* #line 145 "RST.c.rl" */



/* #line 16 "RST.rl.c" */
static const char _RST_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22, 1, 
	23, 1, 24, 1, 25, 1, 26, 1, 
	27, 1, 28, 1, 29, 1, 30, 1, 
	31, 1, 32, 1, 33, 1, 34, 1, 
	35, 1, 36, 1, 37, 1, 38, 1, 
	39, 1, 40, 1, 41, 1, 42, 1, 
	43, 1, 44, 1, 45, 1, 46, 1, 
	47, 1, 48, 1, 49, 1, 50, 2, 
	2, 3, 2, 2, 4, 2, 2, 5, 
	2, 2, 6, 2, 2, 7, 2, 2, 
	8, 2, 2, 9, 2, 2, 10
};

static const short _RST_key_offsets[] = {
	0, 2, 17, 18, 24, 31, 38, 45, 
	52, 59, 60, 66, 72, 73, 78, 79, 
	94, 95, 101, 108, 115, 122, 129, 136, 
	137, 143, 149, 150, 152, 156, 158, 163, 
	165, 166, 167, 168, 169, 170, 171, 173, 
	177, 179, 184, 186, 187, 188, 189, 190, 
	191, 192, 194, 199, 201, 202, 203, 204, 
	205, 206, 207, 209, 214, 216, 217, 218, 
	219, 220, 221, 222, 224, 229, 231, 232, 
	233, 234, 235, 236, 237, 243, 248, 250, 
	251, 252, 253, 254, 255, 256, 258, 273, 
	274, 280, 287, 294, 301, 308, 315, 316, 
	322, 328, 330, 331, 333, 334, 335, 337, 
	338, 339, 340, 341, 342, 343, 344, 345, 
	375, 378, 379, 381, 383, 390, 392, 399, 
	407, 414, 416, 417, 420, 423, 424, 425, 
	426, 441, 448, 456, 462, 471, 477, 483, 
	489, 499, 510, 511, 513, 515, 522, 532, 
	541
};

static const unsigned char _RST_trans_keys[] = {
	34u, 92u, 10u, 34u, 39u, 48u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 123u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	91u, 95u, 65u, 90u, 97u, 122u, 39u, 10u, 
	34u, 39u, 48u, 92u, 110u, 114u, 117u, 120u, 
	97u, 98u, 101u, 102u, 116u, 118u, 123u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 125u, 48u, 57u, 65u, 70u, 97u, 
	102u, 125u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 42u, 42u, 47u, 
	43u, 45u, 48u, 57u, 48u, 57u, 49u, 51u, 
	54u, 56u, 115u, 50u, 54u, 56u, 50u, 52u, 
	105u, 122u, 101u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 49u, 51u, 54u, 56u, 115u, 
	50u, 54u, 56u, 50u, 52u, 105u, 122u, 101u, 
	48u, 57u, 49u, 51u, 54u, 56u, 115u, 50u, 
	54u, 56u, 50u, 52u, 105u, 122u, 101u, 48u, 
	49u, 49u, 51u, 54u, 56u, 115u, 50u, 54u, 
	56u, 50u, 52u, 105u, 122u, 101u, 48u, 55u, 
	49u, 51u, 54u, 56u, 115u, 50u, 54u, 56u, 
	50u, 52u, 105u, 122u, 101u, 48u, 57u, 65u, 
	70u, 97u, 102u, 49u, 51u, 54u, 56u, 115u, 
	50u, 54u, 56u, 50u, 52u, 105u, 122u, 101u, 
	34u, 92u, 10u, 34u, 39u, 48u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 123u, 48u, 57u, 65u, 70u, 97u, 102u, 
	125u, 48u, 57u, 65u, 70u, 97u, 102u, 125u, 
	48u, 57u, 65u, 70u, 97u, 102u, 125u, 48u, 
	57u, 65u, 70u, 97u, 102u, 125u, 48u, 57u, 
	65u, 70u, 97u, 102u, 125u, 48u, 57u, 65u, 
	70u, 97u, 102u, 125u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	39u, 92u, 34u, 34u, 35u, 34u, 35u, 34u, 
	35u, 34u, 35u, 35u, 34u, 34u, 35u, 35u, 
	35u, 32u, 33u, 34u, 35u, 37u, 38u, 39u, 
	45u, 46u, 47u, 48u, 58u, 60u, 61u, 62u, 
	94u, 95u, 98u, 114u, 124u, 9u, 13u, 42u, 
	43u, 49u, 57u, 65u, 90u, 97u, 122u, 32u, 
	9u, 13u, 61u, 34u, 92u, 33u, 91u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 38u, 61u, 
	39u, 92u, 95u, 65u, 90u, 97u, 122u, 39u, 
	95u, 48u, 57u, 65u, 90u, 97u, 122u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 61u, 62u, 
	46u, 42u, 47u, 61u, 10u, 33u, 47u, 10u, 
	10u, 10u, 46u, 66u, 69u, 79u, 88u, 95u, 
	98u, 101u, 102u, 105u, 111u, 117u, 120u, 48u, 
	57u, 69u, 101u, 102u, 105u, 117u, 48u, 57u, 
	69u, 95u, 101u, 102u, 105u, 117u, 48u, 57u, 
	95u, 102u, 105u, 117u, 48u, 57u, 46u, 69u, 
	95u, 101u, 102u, 105u, 117u, 48u, 57u, 95u, 
	102u, 105u, 117u, 48u, 57u, 95u, 102u, 105u, 
	117u, 48u, 49u, 95u, 102u, 105u, 117u, 48u, 
	55u, 95u, 102u, 105u, 117u, 48u, 57u, 65u, 
	70u, 97u, 101u, 95u, 102u, 105u, 115u, 117u, 
	48u, 57u, 65u, 70u, 97u, 101u, 58u, 60u, 
	61u, 61u, 62u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 34u, 39u, 95u, 114u, 48u, 57u, 
	65u, 90u, 97u, 122u, 34u, 35u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 61u, 124u, 0
};

static const char _RST_single_lengths[] = {
	2, 9, 1, 0, 1, 1, 1, 1, 
	1, 1, 0, 0, 1, 1, 1, 9, 
	1, 0, 1, 1, 1, 1, 1, 1, 
	0, 0, 1, 2, 2, 0, 5, 2, 
	1, 1, 1, 1, 1, 1, 0, 2, 
	0, 5, 2, 1, 1, 1, 1, 1, 
	1, 0, 5, 2, 1, 1, 1, 1, 
	1, 1, 0, 5, 2, 1, 1, 1, 
	1, 1, 1, 0, 5, 2, 1, 1, 
	1, 1, 1, 1, 0, 5, 2, 1, 
	1, 1, 1, 1, 1, 2, 9, 1, 
	0, 1, 1, 1, 1, 1, 1, 0, 
	0, 2, 1, 2, 1, 1, 2, 1, 
	1, 1, 1, 1, 1, 1, 1, 20, 
	1, 1, 2, 2, 1, 2, 3, 2, 
	1, 0, 1, 3, 3, 1, 1, 1, 
	13, 5, 6, 4, 7, 4, 4, 4, 
	4, 5, 1, 2, 2, 1, 4, 3, 
	2
};

static const char _RST_range_lengths[] = {
	0, 3, 0, 3, 3, 3, 3, 3, 
	3, 0, 3, 3, 0, 2, 0, 3, 
	0, 3, 3, 3, 3, 3, 3, 0, 
	3, 3, 0, 0, 1, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 1, 
	1, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 3, 0, 
	3, 3, 3, 3, 3, 3, 0, 3, 
	3, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 5, 
	1, 0, 0, 0, 3, 0, 2, 3, 
	3, 1, 0, 0, 0, 0, 0, 0, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	3, 3, 0, 0, 0, 3, 3, 3, 
	0
};

static const short _RST_index_offsets[] = {
	0, 3, 16, 18, 22, 27, 32, 37, 
	42, 47, 49, 53, 57, 59, 63, 65, 
	78, 80, 84, 89, 94, 99, 104, 109, 
	111, 115, 119, 121, 124, 128, 130, 136, 
	139, 141, 143, 145, 147, 149, 151, 153, 
	157, 159, 165, 168, 170, 172, 174, 176, 
	178, 180, 182, 188, 191, 193, 195, 197, 
	199, 201, 203, 205, 211, 214, 216, 218, 
	220, 222, 224, 226, 228, 234, 237, 239, 
	241, 243, 245, 247, 249, 253, 259, 262, 
	264, 266, 268, 270, 272, 274, 277, 290, 
	292, 296, 301, 306, 311, 316, 321, 323, 
	327, 331, 334, 336, 339, 341, 343, 346, 
	348, 350, 352, 354, 356, 358, 360, 362, 
	388, 391, 393, 396, 399, 404, 407, 413, 
	419, 424, 426, 428, 432, 436, 438, 440, 
	442, 457, 464, 472, 478, 487, 493, 499, 
	505, 513, 522, 524, 527, 530, 535, 543, 
	550
};

static const unsigned char _RST_indicies[] = {
	2, 3, 1, 1, 1, 1, 1, 1, 
	1, 1, 4, 5, 1, 1, 1, 0, 
	6, 0, 7, 7, 7, 0, 1, 8, 
	8, 8, 0, 1, 9, 9, 9, 0, 
	1, 10, 10, 10, 0, 1, 11, 11, 
	11, 0, 1, 12, 12, 12, 0, 1, 
	0, 13, 13, 13, 0, 1, 1, 1, 
	0, 14, 0, 15, 15, 15, 0, 17, 
	16, 18, 18, 18, 18, 18, 18, 18, 
	19, 20, 18, 18, 18, 16, 21, 16, 
	22, 22, 22, 16, 18, 23, 23, 23, 
	16, 18, 24, 24, 24, 16, 18, 25, 
	25, 25, 16, 18, 26, 26, 26, 16, 
	18, 27, 27, 27, 16, 18, 16, 28, 
	28, 28, 16, 18, 18, 18, 16, 30, 
	29, 30, 31, 29, 33, 33, 34, 32, 
	34, 32, 35, 36, 37, 38, 39, 32, 
	40, 38, 32, 38, 32, 38, 32, 38, 
	32, 41, 32, 42, 32, 38, 32, 43, 
	32, 45, 45, 46, 44, 46, 16, 48, 
	49, 50, 51, 52, 47, 53, 51, 47, 
	51, 47, 51, 47, 51, 47, 54, 47, 
	55, 47, 51, 47, 56, 44, 57, 58, 
	59, 60, 61, 44, 62, 60, 44, 60, 
	44, 60, 44, 60, 44, 63, 44, 64, 
	44, 60, 44, 65, 16, 67, 68, 69, 
	70, 71, 66, 72, 70, 66, 70, 66, 
	70, 66, 70, 66, 73, 66, 74, 66, 
	70, 66, 75, 16, 77, 78, 79, 80, 
	81, 76, 82, 80, 76, 80, 76, 80, 
	76, 80, 76, 83, 76, 84, 76, 80, 
	76, 85, 85, 85, 16, 87, 88, 89, 
	90, 91, 86, 92, 90, 86, 90, 86, 
	90, 86, 90, 86, 93, 86, 94, 86, 
	90, 86, 97, 98, 96, 96, 96, 96, 
	96, 96, 96, 96, 99, 100, 96, 96, 
	96, 95, 101, 95, 102, 102, 102, 95, 
	96, 103, 103, 103, 95, 96, 104, 104, 
	104, 95, 96, 105, 105, 105, 95, 96, 
	106, 106, 106, 95, 96, 107, 107, 107, 
	95, 96, 95, 108, 108, 108, 95, 96, 
	96, 96, 95, 95, 109, 18, 111, 110, 
	112, 113, 95, 114, 112, 115, 112, 116, 
	117, 95, 118, 116, 119, 116, 120, 116, 
	121, 95, 122, 121, 123, 121, 124, 121, 
	125, 121, 127, 128, 129, 130, 128, 131, 
	132, 133, 134, 135, 136, 137, 138, 133, 
	139, 128, 140, 141, 142, 143, 127, 128, 
	56, 140, 140, 126, 127, 127, 144, 145, 
	16, 2, 3, 1, 147, 14, 146, 15, 
	15, 15, 15, 148, 145, 145, 146, 146, 
	109, 149, 149, 149, 18, 17, 151, 151, 
	151, 151, 150, 151, 151, 151, 151, 150, 
	145, 146, 153, 152, 29, 154, 145, 146, 
	155, 157, 158, 156, 155, 156, 159, 157, 
	160, 158, 162, 163, 164, 165, 166, 167, 
	163, 164, 168, 168, 165, 168, 166, 56, 
	161, 170, 170, 171, 171, 171, 43, 169, 
	170, 172, 170, 171, 171, 171, 43, 169, 
	33, 171, 171, 171, 34, 169, 162, 164, 
	167, 164, 168, 168, 168, 56, 161, 45, 
	174, 174, 174, 46, 173, 163, 176, 176, 
	176, 65, 175, 165, 178, 178, 178, 75, 
	177, 166, 180, 181, 181, 85, 85, 85, 
	179, 166, 180, 181, 91, 181, 85, 85, 
	85, 179, 145, 146, 153, 145, 146, 145, 
	153, 146, 140, 140, 140, 140, 182, 96, 
	183, 140, 142, 140, 140, 140, 182, 110, 
	184, 140, 140, 140, 140, 182, 145, 145, 
	146, 0
};

static const unsigned char _RST_trans_targs[] = {
	111, 0, 111, 1, 2, 10, 3, 4, 
	5, 6, 7, 8, 9, 11, 13, 116, 
	111, 111, 14, 16, 24, 17, 18, 19, 
	20, 21, 22, 23, 25, 26, 27, 111, 
	111, 29, 131, 31, 33, 34, 111, 35, 
	32, 36, 37, 130, 111, 40, 133, 111, 
	42, 44, 45, 111, 46, 43, 47, 48, 
	132, 51, 53, 54, 111, 55, 52, 56, 
	57, 134, 111, 60, 62, 63, 111, 64, 
	61, 65, 66, 135, 111, 69, 71, 72, 
	111, 73, 70, 74, 75, 136, 111, 78, 
	80, 81, 111, 82, 79, 83, 84, 111, 
	85, 111, 86, 87, 95, 88, 89, 90, 
	91, 92, 93, 94, 96, 15, 98, 111, 
	100, 102, 101, 111, 103, 106, 104, 105, 
	111, 107, 108, 109, 110, 111, 111, 112, 
	113, 114, 115, 117, 118, 121, 122, 123, 
	128, 138, 139, 140, 141, 142, 143, 144, 
	111, 111, 111, 12, 111, 119, 111, 120, 
	111, 113, 124, 111, 125, 126, 127, 111, 
	111, 111, 129, 58, 39, 67, 76, 49, 
	50, 111, 28, 30, 38, 111, 41, 111, 
	59, 111, 68, 111, 137, 77, 111, 97, 
	99
};

static const char _RST_trans_actions[] = {
	83, 0, 19, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	85, 21, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 7, 
	75, 0, 5, 0, 0, 0, 29, 0, 
	0, 0, 0, 5, 79, 0, 96, 77, 
	0, 0, 0, 31, 0, 0, 0, 0, 
	99, 0, 0, 0, 33, 0, 0, 0, 
	0, 93, 73, 0, 0, 0, 27, 0, 
	0, 0, 0, 90, 71, 0, 0, 0, 
	25, 0, 0, 0, 0, 87, 69, 0, 
	0, 0, 23, 0, 0, 0, 0, 81, 
	0, 17, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 15, 
	0, 0, 0, 13, 0, 0, 0, 0, 
	11, 0, 0, 0, 0, 9, 37, 0, 
	108, 5, 5, 0, 108, 0, 0, 5, 
	99, 0, 0, 0, 0, 102, 5, 0, 
	67, 35, 63, 0, 59, 0, 45, 0, 
	65, 105, 0, 43, 0, 0, 0, 41, 
	39, 57, 5, 0, 0, 0, 0, 0, 
	0, 53, 0, 0, 0, 55, 0, 51, 
	0, 49, 0, 47, 87, 0, 61, 0, 
	0
};

static const char _RST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const char _RST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0
};

static const short _RST_eof_trans[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 17, 17, 
	17, 17, 17, 17, 17, 17, 17, 17, 
	17, 17, 1, 1, 33, 33, 33, 33, 
	33, 33, 33, 33, 33, 33, 33, 45, 
	17, 48, 48, 48, 48, 48, 48, 48, 
	48, 45, 45, 45, 45, 45, 45, 45, 
	45, 45, 17, 67, 67, 67, 67, 67, 
	67, 67, 67, 17, 77, 77, 77, 77, 
	77, 77, 77, 77, 17, 87, 87, 87, 
	87, 87, 87, 87, 87, 96, 96, 96, 
	96, 96, 96, 96, 96, 96, 96, 96, 
	96, 96, 96, 96, 96, 96, 96, 96, 
	96, 96, 96, 96, 96, 96, 96, 0, 
	145, 17, 147, 147, 149, 147, 147, 151, 
	151, 147, 153, 147, 156, 156, 160, 161, 
	162, 170, 170, 170, 162, 174, 176, 178, 
	180, 180, 147, 147, 147, 183, 183, 183, 
	147
};

static const int RST_start = 111;
static const int RST_first_final = 111;
static const int RST_error = -1;

static const int RST_en_main = 111;


/* #line 148 "RST.c.rl" */

ok64 RSTLexer(RSTstate* state) {

    a_dup(u8c, data, state->data);
    sane($ok(data));

    int cs = 0;
    int act = 0;
    u8c *p = (u8c*) data[0];
    u8c *pe = (u8c*) data[1];
    u8c *eof = pe;
    u8c *ts = NULL;
    u8c *te = NULL;
    ok64 o = OK;

    u8cs tok = {p, p};

    
/* #line 408 "RST.rl.c" */
	{
	cs = RST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 166 "RST.c.rl" */
    
/* #line 414 "RST.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
_resume:
	_acts = _RST_actions + _RST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 431 "RST.rl.c" */
		}
	}

	_keys = _RST_trans_keys + _RST_key_offsets[cs];
	_trans = _RST_index_offsets[cs];

	_klen = _RST_single_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _RST_range_lengths[cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _RST_indicies[_trans];
_eof_trans:
	cs = _RST_trans_targs[_trans];

	if ( _RST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _RST_actions + _RST_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
/* #line 1 "NONE" */
	{te = p+1;}
	break;
	case 3:
/* #line 44 "RST.c.rl" */
	{act = 13;}
	break;
	case 4:
/* #line 44 "RST.c.rl" */
	{act = 14;}
	break;
	case 5:
/* #line 44 "RST.c.rl" */
	{act = 15;}
	break;
	case 6:
/* #line 44 "RST.c.rl" */
	{act = 17;}
	break;
	case 7:
/* #line 44 "RST.c.rl" */
	{act = 18;}
	break;
	case 8:
/* #line 56 "RST.c.rl" */
	{act = 20;}
	break;
	case 9:
/* #line 62 "RST.c.rl" */
	{act = 21;}
	break;
	case 10:
/* #line 62 "RST.c.rl" */
	{act = 22;}
	break;
	case 11:
/* #line 32 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 12:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 13:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 38 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 44 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 44 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 44 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 44 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 44 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 44 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 62 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 62 "RST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 32 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 32 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 32 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 56 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 44 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 44 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 44 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 44 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 44 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 44 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 50 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonAttr(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 56 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 62 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 62 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 68 "RST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 44 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 44 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 44 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 45:
/* #line 44 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 46:
/* #line 44 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 47:
/* #line 44 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 48:
/* #line 56 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 49:
/* #line 62 "RST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 50:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 18:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 20:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 21:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 22:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 904 "RST.rl.c" */
		}
	}

_again:
	_acts = _RST_actions + _RST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
/* #line 915 "RST.rl.c" */
		}
	}

	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _RST_eof_trans[cs] > 0 ) {
		_trans = _RST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 167 "RST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < RST_first_final)
        o = RSTBAD;

    return o;
}
