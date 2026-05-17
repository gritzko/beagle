
/* #line 1 "CST.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "CST.h"

ok64 CSTonComment (u8cs tok, CSTstate* state);
ok64 CSTonString (u8cs tok, CSTstate* state);
ok64 CSTonNumber (u8cs tok, CSTstate* state);
ok64 CSTonPreproc (u8cs tok, CSTstate* state);
ok64 CSTonWord (u8cs tok, CSTstate* state);
ok64 CSTonPunct (u8cs tok, CSTstate* state);
ok64 CSTonSpace (u8cs tok, CSTstate* state);


/* #line 141 "CST.c.rl" */



/* #line 16 "CST.rl.c" */
static const char _CST_actions[] = {
	0, 1, 2, 1, 3, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	18, 1, 19, 1, 20, 1, 21, 1, 
	22, 1, 23, 1, 24, 1, 25, 1, 
	26, 1, 27, 1, 28, 1, 29, 1, 
	30, 1, 31, 1, 32, 1, 33, 1, 
	34, 1, 35, 1, 36, 1, 37, 1, 
	38, 1, 39, 1, 40, 1, 41, 1, 
	42, 1, 43, 1, 44, 1, 45, 1, 
	46, 1, 47, 1, 48, 1, 49, 1, 
	50, 2, 0, 1, 2, 3, 4, 2, 
	3, 5, 2, 3, 6, 2, 3, 7, 
	2, 3, 8, 2, 3, 9, 2, 3, 
	10, 2, 3, 11, 2, 3, 12
};

static const short _CST_key_offsets[] = {
	0, 0, 2, 4, 20, 26, 32, 38, 
	44, 50, 56, 62, 68, 69, 70, 71, 
	87, 89, 105, 111, 117, 123, 129, 135, 
	141, 147, 153, 155, 171, 177, 183, 189, 
	195, 201, 207, 213, 219, 220, 224, 226, 
	228, 229, 231, 235, 237, 239, 243, 245, 
	247, 249, 255, 261, 262, 293, 296, 297, 
	298, 314, 321, 329, 337, 345, 353, 361, 
	371, 380, 388, 396, 405, 413, 421, 429, 
	437, 445, 453, 461, 469, 477, 485, 493, 
	501, 509, 517, 525, 533, 541, 549, 557, 
	565, 573, 581, 589, 597, 605, 613, 621, 
	629, 630, 632, 634, 637, 640, 651, 660, 
	663, 665, 666, 667, 681, 691, 702, 711, 
	721, 730, 734, 736, 738, 740, 747, 751, 
	753, 755, 757, 768, 772, 774, 776, 778, 
	779, 781, 783, 785, 787, 788, 795, 802
};

static const unsigned char _CST_trans_keys[] = {
	34u, 92u, 34u, 92u, 34u, 39u, 48u, 63u, 
	85u, 92u, 110u, 114u, 117u, 120u, 97u, 98u, 
	101u, 102u, 116u, 118u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 48u, 57u, 65u, 70u, 
	97u, 102u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 48u, 57u, 
	65u, 70u, 97u, 102u, 34u, 34u, 34u, 9u, 
	32u, 95u, 100u, 101u, 105u, 108u, 110u, 112u, 
	114u, 117u, 119u, 65u, 90u, 97u, 122u, 34u, 
	92u, 34u, 39u, 48u, 63u, 85u, 92u, 110u, 
	114u, 117u, 120u, 97u, 98u, 101u, 102u, 116u, 
	118u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 39u, 92u, 34u, 39u, 48u, 63u, 85u, 
	92u, 110u, 114u, 117u, 120u, 97u, 98u, 101u, 
	102u, 116u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 46u, 43u, 45u, 48u, 57u, 
	48u, 57u, 48u, 57u, 42u, 42u, 47u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 43u, 
	45u, 48u, 57u, 48u, 57u, 48u, 57u, 48u, 
	49u, 48u, 57u, 65u, 70u, 97u, 102u, 34u, 
	95u, 65u, 90u, 97u, 122u, 34u, 32u, 34u, 
	35u, 36u, 38u, 39u, 42u, 43u, 45u, 46u, 
	47u, 48u, 58u, 60u, 61u, 62u, 63u, 64u, 
	94u, 95u, 124u, 9u, 13u, 33u, 37u, 49u, 
	57u, 65u, 90u, 97u, 122u, 32u, 9u, 13u, 
	61u, 34u, 9u, 32u, 95u, 100u, 101u, 105u, 
	108u, 110u, 112u, 114u, 117u, 119u, 65u, 90u, 
	97u, 122u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 101u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 102u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 105u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 110u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 101u, 48u, 57u, 65u, 90u, 97u, 
	122u, 95u, 108u, 110u, 114u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 105u, 115u, 48u, 57u, 
	65u, 90u, 97u, 122u, 95u, 102u, 48u, 57u, 
	65u, 90u, 97u, 122u, 95u, 100u, 48u, 57u, 
	65u, 90u, 97u, 122u, 95u, 105u, 114u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 101u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 103u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 105u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 111u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 110u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 114u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 111u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 114u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 117u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 108u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 108u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 97u, 48u, 
	57u, 65u, 90u, 98u, 122u, 95u, 98u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 108u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 114u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 97u, 48u, 
	57u, 65u, 90u, 98u, 122u, 95u, 103u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 109u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 97u, 48u, 
	57u, 65u, 90u, 98u, 122u, 95u, 110u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 100u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 101u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 97u, 48u, 
	57u, 65u, 90u, 98u, 122u, 95u, 114u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 110u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 105u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 110u, 48u, 
	57u, 65u, 90u, 97u, 122u, 95u, 103u, 48u, 
	57u, 65u, 90u, 97u, 122u, 34u, 38u, 61u, 
	43u, 61u, 45u, 61u, 62u, 46u, 48u, 57u, 
	69u, 77u, 95u, 101u, 109u, 48u, 57u, 68u, 
	70u, 100u, 102u, 68u, 70u, 77u, 95u, 100u, 
	102u, 109u, 48u, 57u, 42u, 47u, 61u, 10u, 
	47u, 10u, 10u, 46u, 66u, 69u, 76u, 85u, 
	88u, 95u, 98u, 101u, 108u, 117u, 120u, 48u, 
	57u, 69u, 77u, 101u, 109u, 48u, 57u, 68u, 
	70u, 100u, 102u, 69u, 77u, 95u, 101u, 109u, 
	48u, 57u, 68u, 70u, 100u, 102u, 68u, 70u, 
	77u, 95u, 100u, 102u, 109u, 48u, 57u, 46u, 
	69u, 76u, 85u, 95u, 101u, 108u, 117u, 48u, 
	57u, 68u, 70u, 77u, 95u, 100u, 102u, 109u, 
	48u, 57u, 76u, 85u, 108u, 117u, 85u, 117u, 
	76u, 108u, 76u, 108u, 76u, 85u, 95u, 108u, 
	117u, 48u, 49u, 76u, 85u, 108u, 117u, 85u, 
	117u, 76u, 108u, 76u, 108u, 76u, 85u, 95u, 
	108u, 117u, 48u, 57u, 65u, 70u, 97u, 102u, 
	76u, 85u, 108u, 117u, 85u, 117u, 76u, 108u, 
	76u, 108u, 58u, 60u, 61u, 61u, 62u, 61u, 
	62u, 46u, 63u, 34u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 95u, 48u, 57u, 65u, 90u, 
	97u, 122u, 61u, 124u, 0
};

static const char _CST_single_lengths[] = {
	0, 2, 2, 10, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 1, 1, 12, 
	2, 10, 0, 0, 0, 0, 0, 0, 
	0, 0, 2, 10, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 2, 0, 0, 
	1, 2, 2, 0, 0, 2, 0, 0, 
	0, 0, 2, 1, 21, 1, 1, 1, 
	12, 1, 2, 2, 2, 2, 2, 4, 
	3, 2, 2, 3, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	2, 2, 2, 2, 2, 2, 2, 2, 
	1, 2, 2, 1, 1, 5, 7, 3, 
	2, 1, 1, 12, 4, 5, 7, 8, 
	7, 4, 2, 2, 2, 5, 4, 2, 
	2, 2, 5, 4, 2, 2, 2, 1, 
	2, 0, 2, 2, 1, 1, 1, 2
};

static const char _CST_range_lengths[] = {
	0, 0, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 0, 0, 2, 
	0, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 0, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 1, 1, 1, 
	0, 0, 1, 1, 1, 1, 1, 1, 
	1, 3, 2, 0, 5, 1, 0, 0, 
	2, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	0, 0, 0, 1, 1, 3, 1, 0, 
	0, 0, 0, 1, 3, 3, 1, 1, 
	1, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 3, 0, 0, 0, 0, 0, 
	0, 1, 0, 0, 0, 3, 3, 0
};

static const short _CST_index_offsets[] = {
	0, 0, 3, 6, 20, 24, 28, 32, 
	36, 40, 44, 48, 52, 54, 56, 58, 
	73, 76, 90, 94, 98, 102, 106, 110, 
	114, 118, 122, 125, 139, 143, 147, 151, 
	155, 159, 163, 167, 171, 173, 177, 179, 
	181, 183, 186, 190, 192, 194, 198, 200, 
	202, 204, 208, 213, 215, 242, 245, 247, 
	249, 264, 269, 275, 281, 287, 293, 299, 
	307, 314, 320, 326, 333, 339, 345, 351, 
	357, 363, 369, 375, 381, 387, 393, 399, 
	405, 411, 417, 423, 429, 435, 441, 447, 
	453, 459, 465, 471, 477, 483, 489, 495, 
	501, 503, 506, 509, 512, 515, 524, 533, 
	537, 540, 542, 544, 558, 566, 575, 584, 
	594, 603, 608, 611, 614, 617, 624, 629, 
	632, 635, 638, 647, 652, 655, 658, 661, 
	663, 666, 668, 671, 674, 676, 681, 686
};

static const unsigned char _CST_indicies[] = {
	1, 2, 0, 3, 2, 0, 0, 0, 
	0, 0, 5, 0, 0, 0, 6, 7, 
	0, 0, 0, 4, 8, 8, 8, 4, 
	9, 9, 9, 4, 10, 10, 10, 4, 
	6, 6, 6, 4, 11, 11, 11, 4, 
	12, 12, 12, 4, 7, 7, 7, 4, 
	0, 0, 0, 4, 15, 14, 16, 14, 
	17, 14, 19, 19, 20, 21, 22, 23, 
	24, 25, 26, 27, 28, 29, 20, 20, 
	18, 32, 33, 31, 31, 31, 31, 31, 
	34, 31, 31, 31, 35, 36, 31, 31, 
	31, 30, 37, 37, 37, 30, 38, 38, 
	38, 30, 39, 39, 39, 30, 35, 35, 
	35, 30, 40, 40, 40, 30, 41, 41, 
	41, 30, 36, 36, 36, 30, 31, 31, 
	31, 30, 43, 44, 42, 42, 42, 42, 
	42, 45, 42, 42, 42, 46, 47, 42, 
	42, 42, 4, 48, 48, 48, 4, 49, 
	49, 49, 4, 50, 50, 50, 4, 46, 
	46, 46, 4, 51, 51, 51, 4, 52, 
	52, 52, 4, 47, 47, 47, 4, 42, 
	42, 42, 4, 54, 53, 56, 56, 57, 
	55, 57, 55, 58, 55, 60, 59, 60, 
	61, 59, 63, 63, 64, 62, 64, 62, 
	65, 62, 67, 67, 68, 66, 68, 69, 
	70, 66, 71, 69, 72, 72, 72, 69, 
	73, 74, 74, 74, 4, 75, 73, 77, 
	79, 80, 81, 82, 42, 78, 83, 84, 
	85, 86, 87, 88, 89, 90, 91, 92, 
	93, 78, 94, 95, 77, 78, 70, 94, 
	94, 76, 77, 77, 96, 54, 69, 14, 
	97, 19, 19, 20, 21, 22, 23, 24, 
	25, 26, 27, 28, 29, 20, 20, 98, 
	20, 20, 20, 20, 69, 20, 100, 20, 
	20, 20, 99, 20, 24, 20, 20, 20, 
	99, 20, 101, 20, 20, 20, 99, 20, 
	102, 20, 20, 20, 99, 20, 103, 20, 
	20, 20, 99, 20, 104, 105, 106, 20, 
	20, 20, 99, 20, 23, 102, 20, 20, 
	20, 99, 20, 103, 20, 20, 20, 99, 
	20, 107, 20, 20, 20, 99, 20, 23, 
	27, 20, 20, 20, 99, 20, 108, 20, 
	20, 20, 99, 20, 109, 20, 20, 20, 
	99, 20, 110, 20, 20, 20, 99, 20, 
	111, 20, 20, 20, 99, 20, 103, 20, 
	20, 20, 99, 20, 112, 20, 20, 20, 
	99, 20, 113, 20, 20, 20, 99, 20, 
	103, 20, 20, 20, 99, 20, 114, 20, 
	20, 20, 99, 20, 115, 20, 20, 20, 
	99, 20, 116, 20, 20, 20, 99, 20, 
	117, 20, 20, 20, 99, 20, 118, 20, 
	20, 20, 99, 20, 102, 20, 20, 20, 
	99, 20, 119, 20, 20, 20, 99, 20, 
	120, 20, 20, 20, 99, 20, 121, 20, 
	20, 20, 99, 20, 122, 20, 20, 20, 
	99, 20, 103, 20, 20, 20, 99, 20, 
	123, 20, 20, 20, 99, 20, 124, 20, 
	20, 20, 99, 20, 23, 20, 20, 20, 
	99, 20, 125, 20, 20, 20, 99, 20, 
	126, 20, 20, 20, 99, 20, 127, 20, 
	20, 20, 99, 20, 128, 20, 20, 20, 
	99, 20, 129, 20, 20, 20, 99, 20, 
	103, 20, 20, 20, 99, 31, 130, 54, 
	54, 130, 54, 54, 130, 54, 54, 130, 
	132, 58, 131, 135, 134, 136, 135, 134, 
	58, 134, 134, 133, 134, 134, 134, 56, 
	134, 134, 134, 57, 133, 59, 137, 54, 
	130, 138, 140, 139, 138, 139, 141, 140, 
	143, 144, 145, 146, 147, 148, 149, 144, 
	145, 146, 147, 148, 70, 142, 152, 151, 
	152, 151, 65, 151, 151, 150, 152, 151, 
	153, 152, 151, 65, 151, 151, 150, 151, 
	151, 151, 63, 151, 151, 151, 64, 150, 
	143, 145, 146, 147, 149, 145, 146, 147, 
	70, 142, 155, 155, 155, 67, 155, 155, 
	155, 68, 154, 156, 157, 156, 157, 142, 
	157, 157, 142, 158, 158, 142, 157, 157, 
	142, 160, 161, 144, 160, 161, 71, 159, 
	162, 163, 162, 163, 159, 163, 163, 159, 
	164, 164, 159, 163, 163, 159, 166, 167, 
	148, 166, 167, 72, 72, 72, 165, 168, 
	169, 168, 169, 165, 169, 169, 165, 170, 
	170, 165, 169, 169, 165, 54, 130, 171, 
	54, 130, 54, 130, 54, 171, 130, 54, 
	171, 130, 73, 172, 74, 74, 74, 74, 
	173, 94, 94, 94, 94, 174, 54, 54, 
	130, 0
};

static const unsigned char _CST_trans_targs[] = {
	2, 55, 3, 52, 0, 4, 8, 11, 
	5, 6, 7, 9, 10, 52, 12, 13, 
	14, 52, 52, 15, 57, 58, 63, 65, 
	60, 76, 82, 68, 87, 90, 52, 16, 
	52, 17, 18, 22, 25, 19, 20, 21, 
	23, 24, 26, 52, 27, 28, 32, 35, 
	29, 30, 31, 33, 34, 52, 52, 52, 
	38, 102, 101, 40, 41, 52, 52, 43, 
	110, 109, 52, 46, 112, 52, 111, 117, 
	122, 51, 133, 132, 52, 53, 54, 1, 
	56, 96, 97, 98, 99, 100, 103, 107, 
	127, 128, 129, 130, 131, 50, 134, 135, 
	52, 52, 52, 52, 59, 61, 62, 57, 
	64, 66, 73, 67, 69, 70, 71, 72, 
	74, 75, 77, 78, 79, 80, 81, 83, 
	84, 85, 86, 88, 89, 91, 92, 93, 
	94, 95, 52, 52, 36, 52, 52, 37, 
	39, 104, 52, 105, 106, 52, 52, 108, 
	48, 45, 113, 115, 49, 47, 52, 52, 
	42, 44, 52, 52, 114, 52, 116, 52, 
	118, 120, 119, 52, 121, 52, 123, 125, 
	124, 52, 126, 54, 52, 52, 52
};

static const char _CST_trans_actions[] = {
	0, 3, 0, 11, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 65, 0, 0, 
	0, 7, 73, 0, 102, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 75, 0, 
	9, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 13, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 77, 27, 69, 
	0, 3, 3, 0, 0, 5, 67, 0, 
	3, 3, 71, 0, 93, 79, 96, 90, 
	87, 0, 0, 84, 29, 0, 108, 0, 
	3, 3, 0, 0, 0, 3, 3, 96, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	63, 37, 53, 51, 0, 0, 0, 99, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 59, 61, 0, 45, 21, 0, 
	0, 0, 33, 0, 0, 31, 49, 3, 
	0, 0, 0, 0, 0, 0, 43, 19, 
	0, 0, 47, 23, 0, 25, 0, 41, 
	0, 0, 0, 17, 0, 39, 0, 0, 
	0, 15, 0, 105, 35, 55, 57
};

static const char _CST_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 81, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const char _CST_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0
};

static const short _CST_eof_trans[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 14, 14, 14, 19, 
	31, 31, 31, 31, 31, 31, 31, 31, 
	31, 31, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 54, 56, 56, 56, 
	31, 31, 63, 63, 63, 67, 70, 67, 
	70, 70, 0, 70, 0, 97, 70, 98, 
	99, 70, 100, 100, 100, 100, 100, 100, 
	100, 100, 100, 100, 100, 100, 100, 100, 
	100, 100, 100, 100, 100, 100, 100, 100, 
	100, 100, 100, 100, 100, 100, 100, 100, 
	100, 100, 100, 100, 100, 100, 100, 100, 
	131, 131, 131, 131, 132, 134, 134, 131, 
	139, 139, 142, 143, 151, 151, 151, 143, 
	155, 143, 143, 143, 143, 160, 160, 160, 
	160, 160, 166, 166, 166, 166, 166, 131, 
	131, 131, 131, 131, 173, 174, 175, 131
};

static const int CST_start = 52;
static const int CST_first_final = 52;
static const int CST_error = 0;

static const int CST_en_main = 52;


/* #line 144 "CST.c.rl" */

ok64 CSTLexer(CSTstate* state) {

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

    
/* #line 440 "CST.rl.c" */
	{
	cs = CST_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 162 "CST.c.rl" */
    
/* #line 446 "CST.rl.c" */
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_acts = _CST_actions + _CST_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 465 "CST.rl.c" */
		}
	}

	_keys = _CST_trans_keys + _CST_key_offsets[cs];
	_trans = _CST_index_offsets[cs];

	_klen = _CST_single_lengths[cs];
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

	_klen = _CST_range_lengths[cs];
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
	_trans = _CST_indicies[_trans];
_eof_trans:
	cs = _CST_trans_targs[_trans];

	if ( _CST_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _CST_actions + _CST_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 3:
/* #line 1 "NONE" */
	{te = p+1;}
	break;
	case 4:
/* #line 37 "CST.c.rl" */
	{act = 5;}
	break;
	case 5:
/* #line 43 "CST.c.rl" */
	{act = 9;}
	break;
	case 6:
/* #line 43 "CST.c.rl" */
	{act = 10;}
	break;
	case 7:
/* #line 43 "CST.c.rl" */
	{act = 13;}
	break;
	case 8:
/* #line 43 "CST.c.rl" */
	{act = 14;}
	break;
	case 9:
/* #line 49 "CST.c.rl" */
	{act = 15;}
	break;
	case 10:
/* #line 49 "CST.c.rl" */
	{act = 16;}
	break;
	case 11:
/* #line 61 "CST.c.rl" */
	{act = 20;}
	break;
	case 12:
/* #line 61 "CST.c.rl" */
	{act = 21;}
	break;
	case 13:
/* #line 31 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 14:
/* #line 37 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 15:
/* #line 37 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 37 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 37 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 43 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 43 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 43 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 43 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 43 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 43 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 61 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 61 "CST.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 31 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 31 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 37 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 37 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 43 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 43 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 43 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 43 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 43 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 43 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 49 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 61 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 55 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 55 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 61 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 61 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 67 "CST.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 37 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 43 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 45:
/* #line 43 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 46:
/* #line 43 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 47:
/* #line 61 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 48:
/* #line 61 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 49:
/* #line 61 "CST.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 50:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 0:
	{{cs = 0;goto _again;}}
	break;
	case 5:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 10:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 16:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 20:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 21:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 936 "CST.rl.c" */
		}
	}

_again:
	_acts = _CST_actions + _CST_to_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
/* #line 1 "NONE" */
	{ts = 0;}
	break;
	case 1:
/* #line 1 "NONE" */
	{act = 0;}
	break;
/* #line 950 "CST.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _CST_eof_trans[cs] > 0 ) {
		_trans = _CST_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 163 "CST.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < CST_first_final)
        o = CSTBAD;

    return o;
}
