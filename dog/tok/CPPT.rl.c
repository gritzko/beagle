
/* #line 1 "CPPT.c.rl" */
#include "abc/INT.h"
#include "abc/PRO.h"
#include "CPPT.h"

ok64 CPPTonComment (u8cs tok, CPPTstate* state);
ok64 CPPTonString (u8cs tok, CPPTstate* state);
ok64 CPPTonNumber (u8cs tok, CPPTstate* state);
ok64 CPPTonPreproc (u8cs tok, CPPTstate* state);
ok64 CPPTonWord (u8cs tok, CPPTstate* state);
ok64 CPPTonPunct (u8cs tok, CPPTstate* state);
ok64 CPPTonSpace (u8cs tok, CPPTstate* state);


/* #line 140 "CPPT.c.rl" */



/* #line 16 "CPPT.rl.c" */
static const char _CPPT_actions[] = {
	0, 1, 2, 1, 3, 1, 15, 1, 
	16, 1, 17, 1, 18, 1, 19, 1, 
	20, 1, 21, 1, 22, 1, 23, 1, 
	24, 1, 25, 1, 26, 1, 27, 1, 
	28, 1, 29, 1, 30, 1, 31, 1, 
	32, 1, 33, 1, 34, 1, 35, 1, 
	36, 1, 37, 1, 38, 1, 39, 1, 
	40, 1, 41, 1, 42, 1, 43, 1, 
	44, 1, 45, 1, 46, 1, 47, 1, 
	48, 1, 49, 1, 50, 1, 51, 1, 
	52, 2, 0, 1, 2, 3, 4, 2, 
	3, 5, 2, 3, 6, 2, 3, 7, 
	2, 3, 8, 2, 3, 9, 2, 3, 
	10, 2, 3, 11, 2, 3, 12, 2, 
	3, 13, 2, 3, 14
};

static const short _CPPT_key_offsets[] = {
	0, 0, 2, 19, 25, 31, 37, 43, 
	49, 55, 61, 67, 82, 84, 101, 107, 
	113, 119, 125, 131, 137, 143, 149, 150, 
	152, 156, 158, 159, 161, 163, 165, 169, 
	171, 175, 177, 183, 185, 191, 199, 208, 
	214, 218, 220, 221, 222, 223, 256, 259, 
	260, 275, 283, 292, 301, 310, 319, 328, 
	339, 349, 358, 367, 376, 385, 394, 403, 
	413, 423, 432, 441, 450, 459, 468, 477, 
	486, 495, 504, 513, 522, 531, 540, 549, 
	558, 567, 576, 585, 593, 595, 597, 600, 
	603, 614, 623, 626, 627, 640, 647, 657, 
	668, 677, 686, 694, 707, 711, 713, 715, 
	717, 724, 728, 730, 732, 734, 748, 757, 
	761, 763, 765, 767, 768, 770, 771, 773, 
	784, 793, 805
};

static const unsigned char _CPPT_trans_keys[] = {
	34u, 92u, 34u, 39u, 63u, 85u, 92u, 110u, 
	114u, 117u, 120u, 48u, 55u, 97u, 98u, 101u, 
	102u, 116u, 118u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 9u, 32u, 36u, 95u, 100u, 
	101u, 105u, 108u, 112u, 117u, 119u, 65u, 90u, 
	97u, 122u, 39u, 92u, 34u, 39u, 63u, 85u, 
	92u, 110u, 114u, 117u, 120u, 48u, 55u, 97u, 
	98u, 101u, 102u, 116u, 118u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 48u, 57u, 65u, 
	70u, 97u, 102u, 48u, 57u, 65u, 70u, 97u, 
	102u, 48u, 57u, 65u, 70u, 97u, 102u, 48u, 
	57u, 65u, 70u, 97u, 102u, 46u, 48u, 57u, 
	43u, 45u, 48u, 57u, 48u, 57u, 42u, 42u, 
	47u, 48u, 57u, 48u, 57u, 43u, 45u, 48u, 
	57u, 48u, 57u, 43u, 45u, 48u, 57u, 48u, 
	57u, 36u, 95u, 65u, 90u, 97u, 122u, 48u, 
	49u, 48u, 57u, 65u, 70u, 97u, 102u, 80u, 
	112u, 48u, 57u, 65u, 70u, 97u, 102u, 39u, 
	80u, 112u, 48u, 57u, 65u, 70u, 97u, 102u, 
	48u, 57u, 65u, 70u, 97u, 102u, 43u, 45u, 
	48u, 57u, 48u, 57u, 40u, 41u, 34u, 32u, 
	34u, 35u, 36u, 38u, 39u, 42u, 43u, 45u, 
	46u, 47u, 48u, 58u, 60u, 61u, 62u, 76u, 
	82u, 85u, 94u, 95u, 117u, 124u, 9u, 13u, 
	33u, 37u, 49u, 57u, 65u, 90u, 97u, 122u, 
	32u, 9u, 13u, 61u, 9u, 32u, 36u, 95u, 
	100u, 101u, 105u, 108u, 112u, 117u, 119u, 65u, 
	90u, 97u, 122u, 36u, 95u, 48u, 57u, 65u, 
	90u, 97u, 122u, 36u, 95u, 101u, 48u, 57u, 
	65u, 90u, 97u, 122u, 36u, 95u, 102u, 48u, 
	57u, 65u, 90u, 97u, 122u, 36u, 95u, 105u, 
	48u, 57u, 65u, 90u, 97u, 122u, 36u, 95u, 
	110u, 48u, 57u, 65u, 90u, 97u, 122u, 36u, 
	95u, 101u, 48u, 57u, 65u, 90u, 97u, 122u, 
	36u, 95u, 108u, 110u, 114u, 48u, 57u, 65u, 
	90u, 97u, 122u, 36u, 95u, 105u, 115u, 48u, 
	57u, 65u, 90u, 97u, 122u, 36u, 95u, 102u, 
	48u, 57u, 65u, 90u, 97u, 122u, 36u, 95u, 
	100u, 48u, 57u, 65u, 90u, 97u, 122u, 36u, 
	95u, 105u, 48u, 57u, 65u, 90u, 97u, 122u, 
	36u, 95u, 114u, 48u, 57u, 65u, 90u, 97u, 
	122u, 36u, 95u, 111u, 48u, 57u, 65u, 90u, 
	97u, 122u, 36u, 95u, 114u, 48u, 57u, 65u, 
	90u, 97u, 122u, 36u, 95u, 102u, 110u, 48u, 
	57u, 65u, 90u, 97u, 122u, 36u, 95u, 100u, 
	110u, 48u, 57u, 65u, 90u, 97u, 122u, 36u, 
	95u, 101u, 48u, 57u, 65u, 90u, 97u, 122u, 
	36u, 95u, 100u, 48u, 57u, 65u, 90u, 97u, 
	122u, 36u, 95u, 99u, 48u, 57u, 65u, 90u, 
	97u, 122u, 36u, 95u, 108u, 48u, 57u, 65u, 
	90u, 97u, 122u, 36u, 95u, 117u, 48u, 57u, 
	65u, 90u, 97u, 122u, 36u, 95u, 100u, 48u, 
	57u, 65u, 90u, 97u, 122u, 36u, 95u, 114u, 
	48u, 57u, 65u, 90u, 97u, 122u, 36u, 95u, 
	97u, 48u, 57u, 65u, 90u, 98u, 122u, 36u, 
	95u, 103u, 48u, 57u, 65u, 90u, 97u, 122u, 
	36u, 95u, 109u, 48u, 57u, 65u, 90u, 97u, 
	122u, 36u, 95u, 97u, 48u, 57u, 65u, 90u, 
	98u, 122u, 36u, 95u, 110u, 48u, 57u, 65u, 
	90u, 97u, 122u, 36u, 95u, 97u, 48u, 57u, 
	65u, 90u, 98u, 122u, 36u, 95u, 114u, 48u, 
	57u, 65u, 90u, 97u, 122u, 36u, 95u, 110u, 
	48u, 57u, 65u, 90u, 97u, 122u, 36u, 95u, 
	105u, 48u, 57u, 65u, 90u, 97u, 122u, 36u, 
	95u, 110u, 48u, 57u, 65u, 90u, 97u, 122u, 
	36u, 95u, 103u, 48u, 57u, 65u, 90u, 97u, 
	122u, 36u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 38u, 61u, 43u, 61u, 45u, 61u, 62u, 
	46u, 48u, 57u, 39u, 69u, 76u, 101u, 108u, 
	48u, 57u, 68u, 70u, 100u, 102u, 39u, 68u, 
	70u, 76u, 100u, 102u, 108u, 48u, 57u, 42u, 
	47u, 61u, 10u, 39u, 46u, 66u, 69u, 88u, 
	95u, 98u, 101u, 120u, 48u, 55u, 56u, 57u, 
	39u, 46u, 69u, 95u, 101u, 48u, 57u, 69u, 
	76u, 101u, 108u, 48u, 57u, 68u, 70u, 100u, 
	102u, 39u, 69u, 76u, 101u, 108u, 48u, 57u, 
	68u, 70u, 100u, 102u, 39u, 68u, 70u, 76u, 
	100u, 102u, 108u, 48u, 57u, 39u, 68u, 70u, 
	76u, 100u, 102u, 108u, 48u, 57u, 36u, 95u, 
	48u, 57u, 65u, 90u, 97u, 122u, 39u, 46u, 
	69u, 76u, 85u, 95u, 101u, 108u, 117u, 48u, 
	55u, 56u, 57u, 76u, 85u, 108u, 117u, 85u, 
	117u, 76u, 108u, 76u, 108u, 39u, 76u, 85u, 
	108u, 117u, 48u, 49u, 76u, 85u, 108u, 117u, 
	85u, 117u, 76u, 108u, 76u, 108u, 39u, 46u, 
	76u, 80u, 85u, 108u, 112u, 117u, 48u, 57u, 
	65u, 70u, 97u, 102u, 39u, 68u, 70u, 76u, 
	100u, 102u, 108u, 48u, 57u, 76u, 85u, 108u, 
	117u, 85u, 117u, 76u, 108u, 76u, 108u, 58u, 
	60u, 61u, 62u, 61u, 62u, 34u, 36u, 39u, 
	82u, 95u, 48u, 57u, 65u, 90u, 97u, 122u, 
	34u, 36u, 95u, 48u, 57u, 65u, 90u, 97u, 
	122u, 34u, 36u, 39u, 56u, 82u, 95u, 48u, 
	57u, 65u, 90u, 97u, 122u, 61u, 124u, 0
};

static const char _CPPT_single_lengths[] = {
	0, 2, 9, 0, 0, 0, 0, 0, 
	0, 0, 0, 11, 2, 9, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	2, 0, 1, 2, 0, 0, 2, 0, 
	2, 0, 2, 0, 0, 2, 3, 0, 
	2, 0, 1, 1, 1, 23, 1, 1, 
	11, 2, 3, 3, 3, 3, 3, 5, 
	4, 3, 3, 3, 3, 3, 3, 4, 
	4, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 2, 2, 2, 1, 1, 
	5, 7, 3, 1, 9, 5, 4, 5, 
	7, 7, 2, 9, 4, 2, 2, 2, 
	5, 4, 2, 2, 2, 8, 7, 4, 
	2, 2, 2, 1, 2, 1, 2, 5, 
	3, 6, 2
};

static const char _CPPT_range_lengths[] = {
	0, 0, 4, 3, 3, 3, 3, 3, 
	3, 3, 3, 2, 0, 4, 3, 3, 
	3, 3, 3, 3, 3, 3, 0, 1, 
	1, 1, 0, 0, 1, 1, 1, 1, 
	1, 1, 2, 1, 3, 3, 3, 3, 
	1, 1, 0, 0, 0, 5, 1, 0, 
	2, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 3, 3, 3, 3, 
	3, 3, 3, 3, 0, 0, 1, 1, 
	3, 1, 0, 0, 2, 1, 3, 3, 
	1, 1, 3, 2, 0, 0, 0, 0, 
	1, 0, 0, 0, 0, 3, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	3, 3, 0
};

static const short _CPPT_index_offsets[] = {
	0, 0, 3, 17, 21, 25, 29, 33, 
	37, 41, 45, 49, 63, 66, 80, 84, 
	88, 92, 96, 100, 104, 108, 112, 114, 
	116, 120, 122, 124, 127, 129, 131, 135, 
	137, 141, 143, 148, 150, 154, 160, 167, 
	171, 175, 177, 179, 181, 183, 212, 215, 
	217, 231, 237, 244, 251, 258, 265, 272, 
	281, 289, 296, 303, 310, 317, 324, 331, 
	339, 347, 354, 361, 368, 375, 382, 389, 
	396, 403, 410, 417, 424, 431, 438, 445, 
	452, 459, 466, 473, 479, 482, 485, 488, 
	491, 500, 509, 513, 515, 527, 534, 542, 
	551, 560, 569, 575, 587, 592, 595, 598, 
	601, 608, 613, 616, 619, 622, 634, 643, 
	648, 651, 654, 657, 659, 662, 664, 667, 
	676, 683, 693
};

static const unsigned char _CPPT_indicies[] = {
	2, 3, 1, 1, 1, 1, 4, 1, 
	1, 1, 5, 6, 1, 1, 1, 1, 
	0, 7, 7, 7, 0, 8, 8, 8, 
	0, 9, 9, 9, 0, 5, 5, 5, 
	0, 10, 10, 10, 0, 11, 11, 11, 
	0, 6, 6, 6, 0, 1, 1, 1, 
	0, 13, 13, 14, 14, 15, 16, 17, 
	18, 19, 20, 21, 14, 14, 12, 23, 
	24, 22, 22, 22, 22, 25, 22, 22, 
	22, 26, 27, 22, 22, 22, 22, 0, 
	28, 28, 28, 0, 29, 29, 29, 0, 
	30, 30, 30, 0, 26, 26, 26, 0, 
	31, 31, 31, 0, 32, 32, 32, 0, 
	27, 27, 27, 0, 22, 22, 22, 0, 
	34, 33, 36, 35, 37, 37, 38, 35, 
	38, 35, 41, 40, 41, 42, 40, 43, 
	0, 45, 44, 46, 46, 47, 44, 47, 
	44, 48, 48, 49, 0, 49, 0, 50, 
	50, 50, 50, 0, 51, 0, 52, 52, 
	52, 0, 55, 55, 54, 54, 54, 53, 
	56, 55, 55, 54, 54, 54, 53, 54, 
	54, 54, 53, 57, 57, 58, 53, 58, 
	0, 60, 59, 61, 60, 62, 60, 64, 
	1, 66, 67, 68, 22, 65, 69, 70, 
	71, 72, 73, 74, 75, 65, 76, 77, 
	78, 77, 65, 67, 79, 80, 64, 65, 
	43, 67, 67, 63, 64, 64, 81, 34, 
	0, 13, 13, 14, 14, 15, 16, 17, 
	18, 19, 20, 21, 14, 14, 82, 14, 
	14, 14, 14, 14, 0, 14, 14, 84, 
	14, 14, 14, 83, 14, 14, 18, 14, 
	14, 14, 83, 14, 14, 85, 14, 14, 
	14, 83, 14, 14, 86, 14, 14, 14, 
	83, 14, 14, 87, 14, 14, 14, 83, 
	14, 14, 88, 89, 90, 14, 14, 14, 
	83, 14, 14, 91, 86, 14, 14, 14, 
	83, 14, 14, 87, 14, 14, 14, 83, 
	14, 14, 92, 14, 14, 14, 83, 14, 
	14, 91, 14, 14, 14, 83, 14, 14, 
	93, 14, 14, 14, 83, 14, 14, 94, 
	14, 14, 14, 83, 14, 14, 87, 14, 
	14, 14, 83, 14, 14, 95, 96, 14, 
	14, 14, 83, 14, 14, 98, 99, 14, 
	14, 14, 97, 14, 14, 91, 14, 14, 
	14, 83, 14, 14, 98, 14, 14, 14, 
	83, 14, 14, 100, 14, 14, 14, 83, 
	14, 14, 101, 14, 14, 14, 83, 14, 
	14, 102, 14, 14, 14, 83, 14, 14, 
	86, 14, 14, 14, 83, 14, 14, 103, 
	14, 14, 14, 83, 14, 14, 104, 14, 
	14, 14, 83, 14, 14, 105, 14, 14, 
	14, 83, 14, 14, 106, 14, 14, 14, 
	83, 14, 14, 87, 14, 14, 14, 83, 
	14, 14, 99, 14, 14, 14, 83, 14, 
	14, 107, 14, 14, 14, 83, 14, 14, 
	108, 14, 14, 14, 83, 14, 14, 109, 
	14, 14, 14, 83, 14, 14, 110, 14, 
	14, 14, 83, 14, 14, 111, 14, 14, 
	14, 83, 14, 14, 87, 14, 14, 14, 
	83, 67, 67, 67, 67, 67, 112, 34, 
	34, 113, 34, 34, 113, 34, 34, 113, 
	115, 36, 114, 117, 119, 118, 119, 118, 
	36, 118, 118, 116, 37, 118, 118, 118, 
	118, 118, 118, 38, 116, 40, 120, 34, 
	113, 121, 120, 123, 124, 126, 127, 128, 
	129, 126, 127, 128, 125, 43, 122, 123, 
	124, 127, 129, 127, 43, 122, 132, 131, 
	132, 131, 45, 131, 131, 130, 133, 132, 
	131, 132, 131, 45, 131, 131, 130, 46, 
	131, 131, 131, 131, 131, 131, 47, 130, 
	48, 135, 135, 135, 135, 135, 135, 49, 
	134, 50, 50, 50, 50, 50, 122, 123, 
	124, 127, 137, 138, 129, 127, 137, 138, 
	125, 43, 136, 139, 140, 139, 140, 136, 
	140, 140, 136, 141, 141, 136, 140, 140, 
	136, 126, 143, 144, 143, 144, 51, 142, 
	145, 146, 145, 146, 142, 146, 146, 142, 
	147, 147, 142, 146, 146, 142, 128, 149, 
	150, 55, 151, 150, 55, 151, 52, 52, 
	52, 148, 57, 153, 153, 153, 153, 153, 
	153, 58, 152, 154, 155, 154, 155, 148, 
	155, 155, 148, 156, 156, 148, 155, 155, 
	148, 34, 113, 157, 158, 113, 34, 159, 
	34, 157, 113, 1, 67, 22, 78, 67, 
	67, 67, 67, 112, 160, 67, 67, 67, 
	67, 67, 112, 1, 67, 22, 77, 78, 
	67, 67, 67, 67, 112, 34, 34, 113, 
	0
};

static const char _CPPT_trans_targs[] = {
	45, 1, 45, 2, 3, 7, 10, 4, 
	5, 6, 8, 9, 45, 11, 49, 50, 
	55, 63, 52, 71, 76, 77, 12, 45, 
	13, 14, 18, 21, 15, 16, 17, 19, 
	20, 45, 45, 45, 88, 25, 89, 45, 
	26, 27, 45, 93, 45, 95, 31, 96, 
	33, 97, 98, 104, 109, 45, 38, 40, 
	39, 41, 110, 45, 43, 44, 45, 45, 
	46, 47, 48, 83, 84, 85, 86, 87, 
	90, 92, 115, 116, 118, 119, 120, 121, 
	122, 45, 45, 45, 51, 53, 54, 49, 
	56, 58, 60, 57, 59, 61, 62, 64, 
	67, 45, 65, 66, 68, 69, 70, 72, 
	73, 74, 75, 78, 79, 80, 81, 82, 
	45, 45, 45, 22, 45, 23, 45, 24, 
	91, 45, 45, 28, 94, 99, 35, 32, 
	36, 34, 45, 45, 30, 29, 45, 45, 
	45, 100, 102, 101, 45, 103, 45, 105, 
	107, 106, 45, 108, 45, 37, 111, 113, 
	45, 45, 112, 45, 114, 47, 117, 45, 
	42
};

static const char _CPPT_trans_actions[] = {
	79, 0, 9, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 71, 0, 105, 0, 
	0, 0, 0, 0, 0, 0, 0, 11, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 77, 27, 69, 3, 0, 3, 75, 
	0, 0, 5, 99, 67, 3, 0, 3, 
	0, 96, 0, 90, 87, 65, 0, 0, 
	0, 0, 84, 73, 0, 0, 7, 29, 
	0, 114, 3, 0, 0, 0, 0, 3, 
	3, 99, 0, 0, 0, 108, 3, 108, 
	0, 63, 53, 51, 0, 0, 0, 102, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 49, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	55, 59, 61, 0, 43, 0, 23, 0, 
	0, 31, 47, 0, 3, 93, 0, 0, 
	0, 0, 41, 21, 0, 0, 45, 25, 
	39, 0, 0, 0, 19, 0, 37, 0, 
	0, 0, 17, 0, 35, 0, 0, 0, 
	33, 13, 0, 15, 0, 111, 0, 57, 
	0
};

static const char _CPPT_to_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 81, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const char _CPPT_from_state_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0
};

static const short _CPPT_eof_trans[] = {
	0, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 13, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 34, 36, 
	36, 36, 40, 40, 1, 45, 45, 45, 
	1, 1, 1, 1, 1, 54, 54, 54, 
	54, 1, 60, 60, 60, 0, 82, 1, 
	83, 1, 84, 84, 84, 84, 84, 84, 
	84, 84, 84, 84, 84, 84, 84, 84, 
	98, 84, 84, 84, 84, 84, 84, 84, 
	84, 84, 84, 84, 84, 84, 84, 84, 
	84, 84, 84, 113, 114, 114, 114, 115, 
	117, 117, 114, 122, 123, 123, 131, 131, 
	131, 135, 123, 137, 137, 137, 137, 137, 
	143, 143, 143, 143, 143, 149, 153, 149, 
	149, 149, 149, 114, 114, 160, 114, 113, 
	113, 113, 114
};

static const int CPPT_start = 45;
static const int CPPT_first_final = 45;
static const int CPPT_error = 0;

static const int CPPT_en_main = 45;


/* #line 143 "CPPT.c.rl" */

ok64 CPPTLexer(CPPTstate* state) {

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

    
/* #line 433 "CPPT.rl.c" */
	{
	cs = CPPT_start;
	ts = 0;
	te = 0;
	act = 0;
	}

/* #line 161 "CPPT.c.rl" */
    
/* #line 439 "CPPT.rl.c" */
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
	_acts = _CPPT_actions + _CPPT_from_state_actions[cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 2:
/* #line 1 "NONE" */
	{ts = p;}
	break;
/* #line 458 "CPPT.rl.c" */
		}
	}

	_keys = _CPPT_trans_keys + _CPPT_key_offsets[cs];
	_trans = _CPPT_index_offsets[cs];

	_klen = _CPPT_single_lengths[cs];
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

	_klen = _CPPT_range_lengths[cs];
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
	_trans = _CPPT_indicies[_trans];
_eof_trans:
	cs = _CPPT_trans_targs[_trans];

	if ( _CPPT_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _CPPT_actions + _CPPT_trans_actions[_trans];
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
/* #line 47 "CPPT.c.rl" */
	{act = 6;}
	break;
	case 5:
/* #line 47 "CPPT.c.rl" */
	{act = 7;}
	break;
	case 6:
/* #line 47 "CPPT.c.rl" */
	{act = 8;}
	break;
	case 7:
/* #line 47 "CPPT.c.rl" */
	{act = 9;}
	break;
	case 8:
/* #line 47 "CPPT.c.rl" */
	{act = 12;}
	break;
	case 9:
/* #line 47 "CPPT.c.rl" */
	{act = 13;}
	break;
	case 10:
/* #line 53 "CPPT.c.rl" */
	{act = 14;}
	break;
	case 11:
/* #line 53 "CPPT.c.rl" */
	{act = 15;}
	break;
	case 12:
/* #line 59 "CPPT.c.rl" */
	{act = 17;}
	break;
	case 13:
/* #line 65 "CPPT.c.rl" */
	{act = 18;}
	break;
	case 14:
/* #line 65 "CPPT.c.rl" */
	{act = 19;}
	break;
	case 15:
/* #line 35 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 16:
/* #line 41 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 17:
/* #line 41 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 18:
/* #line 41 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonString(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 19:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 20:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 21:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 22:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 23:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 24:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 25:
/* #line 47 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 26:
/* #line 65 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 27:
/* #line 65 "CPPT.c.rl" */
	{te = p+1;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 28:
/* #line 35 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonComment(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 29:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 30:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 31:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 32:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 33:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 34:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 35:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 36:
/* #line 47 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 37:
/* #line 53 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 38:
/* #line 53 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 39:
/* #line 65 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 40:
/* #line 59 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 41:
/* #line 65 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 42:
/* #line 65 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 43:
/* #line 65 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 44:
/* #line 71 "CPPT.c.rl" */
	{te = p;p--;{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonSpace(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 45:
/* #line 47 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 46:
/* #line 47 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 47:
/* #line 47 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 48:
/* #line 65 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 49:
/* #line 59 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 50:
/* #line 65 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 51:
/* #line 65 "CPPT.c.rl" */
	{{p = ((te))-1;}{
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}}
	break;
	case 52:
/* #line 1 "NONE" */
	{	switch( act ) {
	case 0:
	{{cs = 0;goto _again;}}
	break;
	case 6:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 7:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 8:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 9:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 12:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 13:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonNumber(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 14:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 15:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPreproc(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 17:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonWord(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 18:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	case 19:
	{{p = ((te))-1;}
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CPPTonPunct(tok, state);
    if (o!=OK) {p++; goto _out; }
}
	break;
	}
	}
	break;
/* #line 951 "CPPT.rl.c" */
		}
	}

_again:
	_acts = _CPPT_actions + _CPPT_to_state_actions[cs];
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
/* #line 965 "CPPT.rl.c" */
		}
	}

	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	if ( _CPPT_eof_trans[cs] > 0 ) {
		_trans = _CPPT_eof_trans[cs] - 1;
		goto _eof_trans;
	}
	}

	_out: {}
	}

/* #line 162 "CPPT.c.rl" */

    state->data[0] = p;
    if (o==OK && cs < CPPT_first_final)
        o = CPPTBAD;

    return o;
}
