/////////////////////////////////////////////////////////////////////////////
//
// Υλοποίηση του ADT Map μέσω Hash Table με open addressing (linear probing)
//
/////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <assert.h>

#include "ADTMap.h"


// Οι κόμβοι του map στην υλοποίηση με hash table, μπορούν να είναι σε 3 διαφορετικές καταστάσεις,
// ώστε αν διαγράψουμε κάποιον κόμβο, αυτός να μην είναι empty, ώστε να μην επηρεάζεται η αναζήτηση
// αλλά ούτε occupied, ώστε η εισαγωγή να μπορεί να το κάνει overwrite.
typedef enum {
	EMPTY, OCCUPIED, DELETED
} State;

// Το μέγεθος του Hash Table ιδανικά θέλουμε να είναι πρώτος αριθμός σύμφωνα με την θεωρία.
// Η παρακάτω λίστα περιέχει πρώτους οι οποίοι έχουν αποδεδιγμένα καλή συμπεριφορά ως μεγέθη.
// Κάθε re-hash θα γίνεται βάσει αυτής της λίστας. Αν χρειάζονται παραπάνω απο 1610612741 στοχεία, τότε σε καθε rehash διπλασιάζουμε το μέγεθος.
int prime_sizes[] = {53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241,
	786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

// Χρησιμοποιούμε open addressing, οπότε σύμφωνα με την θεωρία, πρέπει πάντα να διατηρούμε
// τον load factor του  hash table μικρότερο ή ίσο του 0.5, για να έχουμε αποδoτικές πράξεις
#define MAX_LOAD_FACTOR 0.5

// Δομή του κάθε κόμβου που έχει το hash table (με το οποίο υλοιποιούμε το map)
struct map_node{
	Pointer key;		// Το κλειδί που χρησιμοποιείται για να hash-αρουμε
	Pointer value;  	// Η τιμή που αντισοιχίζεται στο παραπάνω κλειδί
	State state;		// Μεταβλητή για να μαρκάρουμε την κατάσταση των κόμβων (βλέπε διαγραφή)
};

// Δομή του Map (περιέχει όλες τις πληροφορίες που χρεαζόμαστε για το HashTable)
struct map {
	MapNode array;				// Ο πίνακας που θα χρησιμοποιήσουμε για το map (remember, φτιάχνουμε ένα hash table)
	int capacity;				// Πόσο χώρο έχουμε δεσμεύσει.
	int size;					// Πόσα στοιχεία έχουμε προσθέσει
	int deleted;				// Πόσα κελιά είναι DELETED
	CompareFunc compare;		// Συνάρτηση για σύγκριση δεικτών, που πρέπει να δίνεται απο τον χρήστη
	HashFunc hash_function;		// Συνάρτηση για να παίρνουμε το hash code του κάθε αντικειμένου.
	DestroyFunc destroy_key;	// Συναρτήσεις που καλούνται όταν διαγράφουμε έναν κόμβο απο το map.
	DestroyFunc destroy_value;

	// Πεδία που έχουμε προσθέσει για το incremental rehash.
	// Προσθέστε επιπλέον πεδία, αν χρειαστούν.
	//
	MapNode old_array;			// Ο παλιός πίνακας από τον οποίο κάνουμε incremental rehash
	int old_capacity;			// Πόσο χώρο είχαμε δεσμεύσει στον παλιό πίνακα
};


Map map_create(CompareFunc compare, DestroyFunc destroy_key, DestroyFunc destroy_value) {
	// Δεσμεύουμε κατάλληλα τον χώρο που χρειαζόμαστε για το hash table
	Map map = malloc(sizeof(*map));
	map->capacity = prime_sizes[0];
	map->array = malloc(map->capacity * sizeof(struct map_node));

	// Σε ένα καινούριο map ο παλιός πίνακας είναι απλά κενός
	map->old_capacity = 0;
	map->old_array = NULL;

	// Αρχικοποιούμε τους κόμβους που έχουμε σαν διαθέσιμους.
	for (int i = 0; i < map->capacity; i++)
		map->array[i].state = EMPTY;

	map->size = 0;
	map->deleted = 0;
	map->compare = compare;
	map->destroy_key = destroy_key;
	map->destroy_value = destroy_value;

	return map;
}

// Επιστρέφει τον αριθμό των entries του map σε μία χρονική στιγμή.
int map_size(Map map) {
	return map->size;
}

// Εισαγωγή στο hash table του ζευγαριού (key, item). Αν το key υπάρχει,
// ανανέωση του με ένα νέο value, και η συνάρτηση επιστρέφει true.
int prime=0; //βοηθητική μεταβλητη για το rehash
int incremental_counter;
bool flag=false;
void map_insert(Map map, Pointer key, Pointer value) {
	// Σκανάρουμε το Hash Table μέχρι να βρούμε διαθέσιμη θέση για να τοποθετήσουμε το ζευγάρι,
	// ή μέχρι να βρούμε το κλειδί ώστε να το αντικαταστήσουμε.
	bool already_in_map = false;
	MapNode node = NULL;
	uint pos;
	for (pos = map->hash_function(key) % map->capacity;		// ξεκινώντας από τη θέση που κάνει hash το key
		map->array[pos].state != EMPTY;						// αν φτάσουμε σε EMPTY σταματάμε
		pos = (pos + 1) % map->capacity) {					// linear probing, γυρνώντας στην αρχή όταν φτάσουμε στη τέλος του πίνακα

		if (map->array[pos].state == DELETED) {
			// Βρήκαμε DELETED θέση. Θα μπορούσαμε να βάλουμε το ζευγάρι εδώ, αλλά _μόνο_ αν το key δεν υπάρχει ήδη.
			// Οπότε σημειώνουμε τη θέση, αλλά συνεχίζουμε την αναζήτηση, το key μπορεί να βρίσκεται πιο μετά.
			if (node == NULL)
				node = &map->array[pos];

		} else if (map->compare(map->array[pos].key, key) == 0) {
			already_in_map = true;
			node = &map->array[pos];						// βρήκαμε το key, το ζευγάρι θα μπει αναγκαστικά εδώ (ακόμα και αν είχαμε προηγουμένως βρει DELETED θέση)
			break;											// και δε χρειάζεται να συνεχίζουμε την αναζήτηση.
		}
	}
	if (node == NULL)										// αν βρήκαμε EMPTY (όχι DELETED, ούτε το key), το node δεν έχει πάρει ακόμα τιμή
		node = &map->array[pos];

	// Σε αυτό το σημείο, το node είναι ο κόμβος στον οποίο θα γίνει εισαγωγή.
	if (already_in_map) {
		// Αν αντικαθιστούμε παλιά key/value, τa κάνουμε destropy
		if (node->key != key && map->destroy_key != NULL)
			map->destroy_key(node->key);

		if (node->value != value && map->destroy_value != NULL)
			map->destroy_value(node->value);

	} else {
		// Νέο στοιχείο, αυξάνουμε τα συνολικά στοιχεία του map
		map->size++;

		if (node->state == DELETED)							// αν βρήκαμε DELETED, θα αλλάξει σε OCCUPIED
			map->deleted--;
	}

	// Προσθήκη τιμών στον κόμβο
	node->state = OCCUPIED;
	node->key = key;
	node->value = value;

	// Αν με την νέα εισαγωγή ξεπερνάμε το μέγιστο load factor, πρέπει να κάνουμε rehash.
	// Στο load factor μετράμε και τα DELETED, γιατί και αυτά επηρρεάζουν τις αναζητήσεις.
	float load_factor = (float)(map->size + map->deleted) / map->capacity;
	if (load_factor > MAX_LOAD_FACTOR) {
		// TODO
		//
		// Στην κλασική υλοποίηση εδώ κάναμε το rehash. Τώρα θα πρέπει να
		// ξεκινήσουμε το incremental rehash, και να το συνεχίζουμε σε κάθε
		// μελλοντικό insert.
		map->old_array=map->array;
		map->old_capacity=map->capacity;
		if(prime < 25)
			map->capacity=prime_sizes[++prime];
		else
			map->capacity=2*(map->capacity);
		incremental_counter=0;
		flag=true;
		// Το παρακάτω assert ελέγχει ότι δεν ξεπερνάμε το μέγιστο συντελεστή
		// πληρότητας, θα αποτύχει μέχρι να υλοποιηθεί το incremental rehash.
		assert((float)(map->size + map->deleted) / map->capacity <= MAX_LOAD_FACTOR);
	}
	if(flag==true && incremental_counter<map->old_capacity){
		for(int i=0; i<2; i++){
			map_insert(map, map->old_array[incremental_counter].key, map->old_array[incremental_counter].value);
			incremental_counter++;
			if(incremental_counter == map->old_capacity){
				flag=false;
				break;
			}
		}
	}
}

// Διαργραφή απο το Hash Table του κλειδιού με τιμή key
bool map_remove(Map map, Pointer key) {
	MapNode node = map_find_node(map, key);
	if (node == MAP_EOF)
		return false;

	// destroy
	if (map->destroy_key != NULL)
		map->destroy_key(node->key);
	if (map->destroy_value != NULL)
		map->destroy_value(node->value);

	// θέτουμε ως "deleted", ώστε να μην διακόπτεται η αναζήτηση, αλλά ταυτόχρονα να γίνεται ομαλά η εισαγωγή
	node->state = DELETED;
	map->deleted++;
	map->size--;

	return true;
}

// Αναζήτηση στο map, με σκοπό να επιστραφεί το value του κλειδιού που περνάμε σαν όρισμα.

Pointer map_find(Map map, Pointer key) {
	MapNode node = map_find_node(map, key);
	if (node != MAP_EOF)
		return node->value;
	else
		return NULL;
}


DestroyFunc map_set_destroy_key(Map map, DestroyFunc destroy_key) {
	DestroyFunc old = map->destroy_key;
	map->destroy_key = destroy_key;
	return old;
}

DestroyFunc map_set_destroy_value(Map map, DestroyFunc destroy_value) {
	DestroyFunc old = map->destroy_value;
	map->destroy_value = destroy_value;
	return old;
}

// Απελευθέρωση μνήμης που δεσμεύει το map
void map_destroy(Map map) {
	for (int i = 0; i < map->capacity; i++) {
		if (map->array[i].state == OCCUPIED) {
			if (map->destroy_key != NULL)
				map->destroy_key(map->array[i].key);
			if (map->destroy_value != NULL)
				map->destroy_value(map->array[i].value);
		}
	}

	free(map->array);
	free(map);
}

/////////////////////// Διάσχιση του map μέσω κόμβων ///////////////////////////

MapNode map_first(Map map) {
	//Ξεκινάμε την επανάληψή μας απο το 1ο στοιχείο, μέχρι να βρούμε κάτι όντως τοποθετημένο
	for (int i = 0; i < map->capacity; i++)
		if (map->array[i].state == OCCUPIED)
			return &map->array[i];

	return MAP_EOF;
}

MapNode map_next(Map map, MapNode node) {
	// Το node είναι pointer στο i-οστό στοιχείο του array, οπότε node - array == i  (pointer arithmetic!)
	for (int i = node - map->array + 1; i < map->capacity; i++)
		if (map->array[i].state == OCCUPIED)
			return &map->array[i];

	return MAP_EOF;
}

Pointer map_node_key(Map map, MapNode node) {
	return node->key;
}

Pointer map_node_value(Map map, MapNode node) {
	return node->value;
}

MapNode map_find_node(Map map, Pointer key) {
	// Διασχίζουμε τον πίνακα, ξεκινώντας από τη θέση που κάνει hash το key, και για όσο δε βρίσκουμε EMPTY
	int count = 0;
	for (uint pos = map->hash_function(key) % map->capacity;		// ξεκινώντας από τη θέση που κάνει hash το key
		map->array[pos].state != EMPTY;							// αν φτάσουμε σε EMPTY σταματάμε
		pos = (pos + 1) % map->capacity) {						// linear probing, γυρνώντας στην αρχή όταν φτάσουμε στη τέλος του πίνακα

		// Μόνο σε OCCUPIED θέσεις (όχι DELETED), ελέγχουμε αν το key είναι εδώ
		if (map->array[pos].state == OCCUPIED && map->compare(map->array[pos].key, key) == 0)
			return &map->array[pos];

		// Αν διασχίσουμε ολόκληρο τον πίνακα σταματάμε. Εφόσον ο πίνακας δεν μπορεί να είναι όλος OCCUPIED,
		// αυτό μπορεί να συμβεί μόνο στην ακραία περίπτωση που ο πίνακας έχει γεμίσει DELETED τιμές!
		count++;
		if (count == map->capacity)
			break;
	}

	return MAP_EOF;
}

// Αρχικοποίηση της συνάρτησης κατακερματισμού του συγκεκριμένου map.
void map_set_hash_function(Map map, HashFunc func) {
	map->hash_function = func;
}

uint hash_string(Pointer value) {
	// djb2 hash function, απλή, γρήγορη, και σε γενικές γραμμές αποδοτική
    uint hash = 5381;
    for (char* s = value; *s != '\0'; s++)
		hash = (hash << 5) + hash + *s;			// hash = (hash * 33) + *s. Το foo << 5 είναι γρηγορότερη εκδοχή του foo * 32.
    return hash;
}

uint hash_int(Pointer value) {
	return *(int*)value;
}

uint hash_pointer(Pointer value) {
	return (size_t)value;				// cast σε sizt_t, που έχει το ίδιο μήκος με έναν pointer
}
