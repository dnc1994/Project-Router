/*
This file contains the source code of my Router project for my Data Structure course.
Each line of code in this file was my original work, inspired by several papers which I had listed as references in my report.
Author: 章凌豪 / Zhang Linghao <zlhdnc1994@gmail.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

// v4 version with fixed STRIDE
#define LENGTH 32 // set this to 128 when applied to IPv6
#define STRIDE 5
#define LEVEL (LENGTH / STRIDE + 1)

#define STRPOW (1 << STRIDE)
#define BITS LEVEL

typedef unsigned long mask_t; // since ipv4 addr casted to binary ranges from 0 ~ 2^32 - 1
typedef unsigned char bits_t; // since each stride of ip addr after the cast ranges from 0 ~ 2^STRIDE - 1
typedef unsigned short hop_t; // since the port to next hop is represented by at most 2 letters

#define mask1 ((mask_t)1)

// Dynamic Tree BitMap
typedef struct DTBM_Node {
	mask_t imap; // internal map
	struct DTBM_Node* emap[STRPOW]; // external map
	int ecnt; // count non-zero bits in external map
	hop_t hops[STRPOW];
} DTBM_Node;

// MemList are used for allocating & deallocating DTBM_Nodes
/*
struct MemList_Node {
	MemList_Node *next;
	DTBM_Node *ptr;
};
*/

//MemList_Node *free_nodes_list; // free_nodes_list points to the head of available nodes

DTBM_Node *root;

FILE *fin1, *fin2, *fout;

int global_cnt = 0;
int count[4];

double t_start, t_finish;

int PROFILE_READ_TIME = 0;

DTBM_Node *new_node() {
	DTBM_Node *x;
	/*
	if (free_nodes_list == NULL) {
		x = (*DTBM_Node)malloc(sizeof(DTBM_Node));
		x->imap = x->cnt = 0;
		memset(x->emap, 0, sizeof(x->emap));
		memset(x->hops, 0, sizeof(x->hops));
		free_nodes_list = x;
	}
	else {
		free_nodes_list = x->next;
	}
	*/
	x = (DTBM_Node*)malloc(sizeof(DTBM_Node));
	x->imap = x->ecnt = 0;
	memset(x->emap, 0, sizeof(x->emap));
	memset(x->hops, 0, sizeof(x->hops));
	//++ global_cnt;
	return x;
}

void free_node(DTBM_Node *x) {
	/*
	if (free_nodes_list == NULL) free_nodes_list = x;
	else {
		x->next = free_nodes_list;
		free_nodes_list = x;
	}
	x->imap = x->ecnt = 0;
	memset(x->emap, 0, sizeof(x->emap));
	memset(x->hops, 0, sizeof(x->hops));
	*/
	free(x);
}

// check if the pos-th bit of mask is 1
inline int check_bit(mask_t mask, int pos) {
	return (mask & (mask1 << (pos - 1)));
}

// find the length of longest matching prefix inside the node x
int lmp_len(DTBM_Node *x, bits_t bit, int ip_len) {
	// i = 2^H + ip.bits(h) = 2^(S-1) + (ip.bits(S) >> 1)
	int H = STRIDE - 1;
	int i = (1 << H) + (bit >> 1), ret = H;
	// search bottom-up, utilizing the trait of full binary tree
	while (i > 0 && !check_bit(x->imap, i)) {
		i >>= 1;
		ret --;
	}
	return ret;
}

void lookup_ip(bits_t bits[], int ip_len) {
	DTBM_Node *cur, *bmp;
	int idx, hop_idx, lmp;
	for (idx = 0, cur = root; cur != NULL; ++ idx, ip_len -= STRIDE) {
		lmp = lmp_len(cur, bits[idx], ip_len);
		if (lmp >= 0) {
			// update best matching prefix
			bmp = cur;
			hop_idx = (1 << lmp) + (bits[idx] >> (STRIDE - lmp));
		}
		cur = cur->emap[bits[idx]];
	}
	// in case there is no bmp, although the data is supposed to avoid that
	if (bmp == NULL) {
		fputc('\n', fout);
		printf("NOT FOUND!\n");
		return;
	}
	hop_t nxt_hop = bmp->hops[hop_idx];
	for (; nxt_hop; nxt_hop >>= 8)
		// mask out the most significant 8 bits
		fputc(nxt_hop & (hop_t)(unsigned char)(-1), fout);
	fputc('\n', fout);
}

void insert_entry(bits_t bits[], int prefix_len, hop_t nxt_hop) {
	if (root == NULL) root = new_node();
	DTBM_Node *cur, *tmp;
	int idx, ipos;
	for (idx = 0, cur = root; prefix_len >= STRIDE && (tmp = cur->emap[bits[idx]]) != NULL; ++ idx) {
		cur = tmp;
		prefix_len -= STRIDE;
	}
	if (prefix_len >= STRIDE) {
		cur->ecnt ++;
		for (; prefix_len >= STRIDE; ++ idx) {
			// create new child node
			cur = cur->emap[bits[idx]] = new_node();
			cur->ecnt = 1;
			prefix_len -= STRIDE;
		}
	}
	if (!prefix_len) ipos = 1;
	else ipos = (1 << prefix_len) + (bits[idx] >> (STRIDE - prefix_len));
	// update internal map and the next hop information
	cur->imap |= (mask1 << (ipos - 1));
	cur->hops[ipos] = nxt_hop;
}

void delete_entry(bits_t bits[], int prefix_len) {
	DTBM_Node *cur, *x, *node_stk[LEVEL];
	int idx, xb, top, idx_stk[LEVEL], ipos;
	for (idx = 0, cur = root, x = NULL; prefix_len >= STRIDE && cur != NULL; ++ idx) {
		if (cur->ecnt == 1 && cur->imap == 0) {
			top ++;
			node_stk[top] = cur, idx_stk[top] = bits[idx];
		}
		else {
			top = -1;
			x = cur, xb = bits[idx];
		}
		cur = cur->emap[bits[idx]];
		prefix_len -= STRIDE;
	}
	if (cur != NULL) {
		if (!prefix_len) ipos = 1;
		else ipos = (1 << prefix_len) + (bits[idx] >> (STRIDE - prefix_len));
		if (!check_bit(cur->imap, ipos)) return;
		// delete prefix from node cur
		cur->imap &= ~(mask1 << (ipos - 1));
		cur->hops[ipos] = 0;
		if (cur->ecnt || cur->imap) return;
		// else if node cur's internal map is 0 and no children, then node cur along with its contiguous ancestors with ecnt == 1 can be deleted
		free_node(cur);
		while (top >= 0) {
			cur = node_stk[top];
			cur->emap[idx_stk[top]] = NULL;
			cur->ecnt = 0;
			free_node(cur);
			top --;
		}
		// else we only need to delete node cur and reduce the ecnt of its nearest ancestor by 1
		if (x != NULL) {
			x->emap[xb] = NULL;
			x->ecnt --;
		}
		// else it means the DTBM is deleted to empty
		else root = NULL;
	}
}

// There may be other ways to construct RIB more efficiently.
// For instance, Sartaj Sahni & Kun Suk Kim proposed in <Efficient Construction Of Multibit Tries For IP Lookup> that optimal value of STRIDE can be determined by dynamic programming.
// However updates to the routing table destories the optimality.
// Nevertheless if there were many lookups the improvement would still be substantial.
// And maybe we can reconstruct the whole DTBM once in a while by tweaking the DP approach a little bit?
// Yet I doubt I would have enough time to try this optimization trick.
void load_RIB() {
	char buffer[80], *i;
	bits_t bits[BITS];
	hop_t nxt_hop;
	int k, nEntry, addr_len, hop_len, bits_idx, shf_bits;
	fscanf(fin1, "%d\n", &nEntry);
	//printf("Loading RIB...\n");
	for (k = 0; k < nEntry; ++ k) {
		/*if (k % 50000 == 0) {
			printf("%d / %d\n", k, nEntry);	
		}*/
		fgets(buffer, 80, fin1);
		memset(bits, 0, sizeof(bits));
		// break addresses into chunks by STRIDE
		for (i = buffer, bits_idx = 0, shf_bits = STRIDE; *i == '0' || *i == '1'; ++ i) {
			bits[bits_idx] |= ((bits_t)(*i - '0')) << (-- shf_bits);
			if (!shf_bits) bits_idx ++, shf_bits = STRIDE;
		}
		addr_len = i - buffer;
		for (++ i; *i != ' '; ++ i);
		for (; *i == ' '; ++ i);
		for (nxt_hop = 0, hop_len = 0; *i != '\n'; ++ i, ++ hop_len)
			nxt_hop |= ((hop_t)*i) << (hop_len << 3);
		insert_entry(bits, addr_len, nxt_hop);
	}
	//printf("Loading completed, %d entries inserted with %d nodes created.\n", nEntry, global_cnt);
}

void operate() {
	char buffer[80], *i;
	bits_t bits[BITS];
	hop_t nxt_hop;
	int k, nOper, oper, addr_len, hop_len, bits_idx, shf_bits;
	fscanf(fin2, "%d\n", &nOper);
	//printf("Processing operations...\n");
	for (k = 0; k < nOper; ++ k) {
		/*if (k % 50000 == 0) {
			printf("%d / %d\n", k, nOper);	
		}*/
		fgets(buffer, 80, fin2);
		memset(bits, 0, sizeof(bits));
		oper = buffer[0] - '0';
		for (i = buffer; *i != ' '; ++ i);
		for (; *i == ' '; ++ i);
		// break addresses into chunks by STRIDE
		for (bits_idx = 0, shf_bits = STRIDE; *i == '0' || *i == '1'; ++ i) {
			bits[bits_idx] |= ((bits_t)(*i - '0')) << (-- shf_bits);
			if (!shf_bits) bits_idx ++, shf_bits = STRIDE;
		}
		++ count[oper];
		addr_len = i - buffer - 2;
		if (oper == 1) lookup_ip(bits, addr_len);
		else {
			for (++ i; *i != ' '; ++ i);
			for (; *i == ' '; ++ i);
			for (nxt_hop = 0, hop_len = 0; *i != '\n'; ++ i, ++ hop_len)
				nxt_hop |= ((hop_t)*i) << (hop_len << 3);
			if (oper == 2) insert_entry(bits, addr_len, nxt_hop);
			else if (oper == 3) delete_entry(bits, addr_len);
		}
	}
}

void init_DTBM() {
	// INIT MemList ...
	root = NULL;
}

int main() {
	fin1 = fopen("RIB.txt", "r");
	fin2 = fopen("oper.txt", "r");
	fout = fopen("output.txt", "w");
	init_DTBM();
	//t_start = clock();
	load_RIB();
	//t_finish = clock();
	//printf("%Time elapsed: %f seconds\n", (double)(t_finish - t_start) / CLOCKS_PER_SEC);
	//t_start = clock();
	operate();
	//t_finish = clock();
	//printf("%Time elapsed: %f seconds\n", (double)(t_finish - t_start) / CLOCKS_PER_SEC);
	/*printf("Lookup: %d\n", count[1]);
	printf("Insertion: %d\n", count[2]);
	printf("Deletion: %d\n", count[3]);*/
	return 0;
};
