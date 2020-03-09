#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define SYMBOLS 256

typedef struct tree
{
	unsigned char symbol;
	int frequency;
	struct tree* left;
	struct tree* right;
}
Tree;

typedef struct plot
{
	Tree* tree;
	struct plot* next;
}
Plot;

typedef struct forest
{
	Plot* first;
}
Forest;

typedef enum { READ, WRITE } mode;

typedef struct
{
    // 8-bit buffer
    unsigned char buffer;
    FILE* stream;
    unsigned char ith;
    
    // number of valid bits in file's second-to-last byte
    unsigned char zth;

    mode io;

    // size of file
    off_t size;
}
Huffile;

typedef struct block
{
	unsigned char ch;
	int freq;
}
block;

typedef struct a
{
	unsigned char ch;
	int value;
	int ith;
	int freq;
	struct a* next;
}compressed;

Forest* mkforest(void);
Tree* pick(Forest* f);
bool plant(Forest* f, Tree* t);
bool rmforest(Forest* f);
Huffile* hfopen(const char* path, const char* mode);
bool trace(Tree* , Huffile*, FILE*);
bool hfclose(Huffile* hf);
Forest* hftree(Forest* f);
block* hash(int ch, block* HASHTABLE);
int bread(Huffile* hf);
Tree* mktree(void);
void rmtree(Tree* t);
compressed* create(void);
compressed* compress(Tree* t, int buffer, int i, compressed* first);

int main(int argc, char* argv[])
{
	// ensures valid input
	if (argc != 4)
	{
		printf("Usage: %s [uncompressed_filename].txt [compressed_filename].bin [output_filename].txt\n", argv[0]);
		return 1;
	}

	// pointer to file which was compressed
	FILE* ifp = fopen(argv[1], "r");
	if (ifp == NULL)
	{
		printf("Unable to open infile.\n");
		return 1;
	}
	// compressed file
	Huffile* hf = hfopen(argv[2], "r");
	if (hf == NULL)
	{
		printf("Unable to open Huffed_file.\n");
		return 2;
	}

	FILE* outfile = fopen(argv[3], "w");
	if (outfile == NULL)
	{
		printf("Unable to open outfile.\n");
		return -3;
	}
	// table contains frequency of every character
	block *table = (block*)malloc(sizeof(block)*SYMBOLS);
	if (table == NULL)
	{
		printf("Could not allocate memory for table.\n");
		return -2;
	}

	compressed* c = create();
	if (c == NULL)
	{
		printf("Could not create c node.\n");
		return 3;
	}
	c->freq = 0;
	
	// contains character tree for which frequency is non-zero
	Forest* f = mkforest();
	if (f == NULL)
	{
		printf("Could not create Forest.\n");
		return -1;
	}

	unsigned char ch; int count=0; char eof;
	// iterating over all the characters and hashing them in the frequency table
	while(ch = fgetc(ifp))
	{
		eof = ch;
		if (eof == EOF)
			break;
		table = hash((int)ch, table);
		if ((int)ch < 0)
		{
			printf("%c", ch);
			return 4;
		}
		if (table == NULL)
		{
			printf("Error while Hashing.\n");
			return 4;
		}
	}
	fseek(ifp, 0, SEEK_SET);

	for (int i=0; i<SYMBOLS; i++)
	{
		if (table[i].freq)
		count++;
	}
	
	for (int i=0; i<SYMBOLS; i++)
	{
		Tree* node = mktree();
		if (node == NULL)
		{
			printf("Unable to make Tree.\n");
			return 6;
		}
		node->symbol = table[i].ch;
		node->frequency = table[i].freq;
		if (!(plant(f, node)) && node->frequency)
		{
			printf("Not able to plant.\n");
			return 5;
		}
	}

	// converts the forest into huffman tree forest
	f = hftree(f);
	if (f == NULL)
	{
		printf("Could not create Huffman Tree.\n");
		return 3;
	}
	// assigning every new character a path in Huffman tree (e.g. 110, 111, 110011, 110010)
	c = compress(f->first->tree, 0, 0, c);
	if (c == NULL)
	{
		printf("Could not assign path to every character.\n");
		return 4;
	}
	compressed* node = c;
	int bit = 0;

	// writing Huffed file using huffman tree
	
	trace(f->first->tree, hf, outfile);
	hfclose(hf);
	rmforest(f);
	free(table);
	fclose(ifp);
	return 0;
}

block* hash(int ch, block* HASHTABLE)
{
	if (HASHTABLE == NULL || ch < 0)
	{
		printf("Did not get HASHTABLE.\n");
		return NULL;
	}
	HASHTABLE[ch].freq += 1;
	HASHTABLE[ch].ch = (char)ch;

	return HASHTABLE;
}

// initialising a tree
Tree* mktree(void)
{
	Tree* tree = malloc(sizeof(Tree));
	if (tree == NULL)
	{
		return NULL;
	}

	tree->symbol = 0x00;
	tree->frequency = 0;
	tree->left = NULL;
	tree->right = NULL;

	return tree;
}

// removing a tree
void rmtree(Tree* t)
{
	if (t == NULL)
	{
		return;
	}

	if (t->left != NULL)
	{
		rmtree(t->left);
	}

	if (t->right != NULL)
	{
		rmtree(t->right);
	}

	free(t);
}

// initialising a tree
Forest* mkforest(void)
{
	Forest *f = malloc(sizeof(Forest));
	if (f == NULL)
	{
		return NULL;
	}

	f->first = NULL;

	return f;
}

// getting the required node
Tree* pick(Forest* f)
{
	if (f == NULL)
	{
		return NULL;
	}

	if (f->first == NULL)
	{
		return NULL;
	}

	Tree* tree = f->first->tree;

	Plot* plot = f->first;
	f->first = f->first->next;
	free(plot);

	return tree;
}

bool plant(Forest* f, Tree* t)
{
	if (f == NULL || t == NULL)
	{
		return false;
	}

	if (t->frequency == 0)
	{
		return false;
	}

	// prepare node for tree
	Plot* plot = malloc(sizeof(Plot));
	if (plot == NULL)
	{
		return false;
	}
	plot->tree = t;

	// find node's position in tree
	Plot* trailer = NULL;
	Plot* leader = f->first;
	while (leader != NULL)
	{
		// keep trees sorted first by frequency then by symbol
		if (t->frequency < leader->tree->frequency)
		{
			break;
		}
		else if (t->frequency == leader->tree->frequency
			&& t->symbol < leader->tree->symbol)
		{
			break;
		}
		else
		{
			trailer = leader;
			leader = leader->next;
		}
	}

	// either insert node at start of Tree
	if (trailer == NULL)
	{
		plot->next = f->first;
		f->first = plot;
	}

	// or insert node in middle or at end of Tree
	else
	{
		plot->next = trailer->next;
		trailer->next = plot;
	}

	return true;
}

bool rmforest(Forest* f)
{
	if (f == NULL)
	{
		return false;
	}

	while (f->first != NULL)
	{
		Plot* plot = f->first;
		f->first = f->first->next;
		rmtree(plot->tree);
		free(plot);
	}

	free(f);
	return true;
}

Forest* hftree(Forest* f)
{
	// ensures Tree is not null
	if (f == NULL)
	{
		return NULL;
	}

	if (f->first == NULL)
    {
        return NULL;
    }
    // ensures that Tree have more than one tree
    if (f->first->next == NULL)
    {
        return f;
    }

	// extract 2 minimum weighted nodes

	Plot* f_plot = f->first;
	Plot* s_plot = f->first->next;

	// make new tree pointing towards above two nodes and have frequency equal to the sum

	Tree* new_tree = mktree();
	
	if (new_tree == NULL)
	{
		return NULL;
	}

	new_tree->frequency = f_plot->tree->frequency + s_plot->tree->frequency;
	new_tree->left = f_plot->tree;
	new_tree->right = s_plot->tree;
	
	for (int i=0; i<2; i++)
	{
		pick(f);
	}
	
	if (!(plant(f, new_tree)))
	{
		printf("Unable to plant Tree in Huffman Forest.\n");
		return NULL;
	}

	return hftree(f);
}

bool trace(Tree* t, Huffile* hf, FILE* f)
{
	if (t == NULL || hf == NULL)
	{
		return false;
	}
	Tree* temp = t;
	int bit;
	while(bit != EOF)
	{
		while(temp->left!=NULL && temp->right!=NULL)
		{
			bit = bread(hf);
			if (bit==1)
				temp = temp->left;
			else
				temp = temp->right;
		}
		fputc(temp->symbol, f);
		temp = t;
	}
	return true;
}

int bread(Huffile* hf)
{
	if (hf == NULL)
	{
		return EOF;
	}

	// check for file's end
	if (ftell(hf->stream) == hf->size - 1)
	{
		if (hf->ith == hf->zth)
		{
			return EOF;
		}
	}

	if (hf->ith == 0)
	{
		if (fread(&hf->buffer, sizeof(hf->buffer), 1, hf->stream) != 1)
		{
			return EOF;
		}
	}

	unsigned char mask = 1 << (7 - hf->ith);

	hf->ith = (hf->ith + 1) % 8;

	return (hf->buffer & mask) ? 1 : 0;
}

Huffile* hfopen(const char* path, const char* mode)
{
	if (path == NULL || mode == NULL)
	{
		return NULL;
	}

	if (strcmp(mode, "r") != 0 && strcmp(mode, "w") != 0)
	{
		return NULL;
	}

	Huffile* hf = malloc(sizeof(Huffile));
	if (hf == NULL)
	{
		return NULL;
	}

	hf->stream = fopen(path, mode);
	if (hf->stream == NULL)
	{
		free(hf);
		return NULL;
	}

	hf->io = (strcmp(mode, "w") == 0) ? WRITE : READ;

	if (hf->io == READ)
	{
		// fast-forward to end of file
		if (fseek(hf->stream, -1, SEEK_END) != 0)
		{
			fclose(hf->stream);
			free(hf);
			return NULL;
		}

		if ((hf->size = ftell(hf->stream) + 1) == 0)
		{
			fclose(hf->stream);
			free(hf);
			return NULL;
		}

		// remember final buffer's index
		if (fread(&hf->zth, sizeof(hf->zth), 1, hf->stream) != 1)
		{
			fclose(hf->stream);
			free(hf);
			return NULL;
		}

		if (fseek(hf->stream, 0, SEEK_SET) != 0)
		{
			fclose(hf->stream);
			free(hf);
			return NULL;
		}
	}

	// initialize state
	hf->buffer = 0x00;
	hf->ith = 0;

	return hf;
}

bool hfclose(Huffile* hf)
{
	if (hf == NULL)
	{
		return false;
	}

	if (hf->stream == NULL)
	{
		return false;
	}

	// be sure final buffer and its index get written out if writing
	if (hf->io == WRITE)
	{
		// write out final buffer if necessary
		if (hf->ith > 0)
		{
			if (fwrite(&hf->buffer, sizeof(hf->buffer), 1, hf->stream) != 1)
			{
				return false;
			}
		}

		// write out final buffer's index
		if (fwrite(&hf->ith, sizeof(hf->ith), 1, hf->stream) != 1)
			return false;
	}

	if (fclose(hf->stream) != 0)
	{
		return false;
	}

	free(hf);
	return true;
}

compressed* create(void)
{
	compressed* node = (compressed*)malloc(sizeof(compressed*));
	if (node == NULL)
	{
		printf("Unable to allocate memory.\n");
		return NULL;
	}
	node->next = NULL;
	node->freq = 0;
	node->value = 0;
	node->ith = 0;
	return node;
}

compressed* fit(compressed* first, compressed* node)
{
	// ensures the Tree(forest) is not NULL
	if (first == NULL)
	{
		return NULL;
	}

	// ensures the node is not NULL
	if (node == NULL)
	{
		return NULL;
	}

	compressed* temp1 = first;
	compressed* temp2 = NULL;

	while(temp1 != NULL)
	{
		if (temp1->freq <= node->freq)
		{
			break;
		}
		temp2 = temp1;
		temp1 = temp1->next;
	}

	if (temp2 == NULL)
	{
		first = node;
		node->next = temp1;
	}
	else
	{
		temp2->next = node;
		node->next = temp1;
	}
	return first;
}

compressed* compress(Tree* t, int buffer, int i, compressed* first)
{
	Tree* temp = t;
	unsigned mask = 1 << i;
	if (temp->left == NULL && temp->right == NULL)
	{
		compressed* node = create();
		if (node == NULL)
		{
			printf("Unable to create compressed node.\n");
			return NULL;
		}
		node->ith = i;
		node->ch = temp->symbol;
		node->value = buffer;
		node->freq = temp->frequency;
		first = fit(first, node);
		if (first == NULL)
		{
			printf("Can not fit.\n");
			return NULL;
		}
		return first;
	}
	else
	{
		if (temp->left != NULL)
		{
			buffer |= mask;
			first = compress(temp->left, buffer, i+1, first);
		}
		if (temp->right != NULL)
		{
			buffer &= ~mask;
			first = compress(temp->right, buffer, i+1, first);
		}
	}
	return first;
}
