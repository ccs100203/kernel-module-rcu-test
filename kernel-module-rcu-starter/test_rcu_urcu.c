#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <urcu.h>
#include <urcu/rculist.h>

struct book {
    int id;
    char name[64];
    char author[64];
    int borrow;
    struct cds_list_head node;
};

static CDS_LIST_HEAD(books);
pthread_spinlock_t booksLock;


int preempt_count() {
    return 0;
}

static void add_book(int id, const char *name, const char *author) {
    struct book *bk;

    bk = malloc(sizeof(struct book));
    if (!bk) {
        return;
    }
    memset(bk, 0, sizeof(struct book));

    bk->id = id;
    strncpy(bk->name, name, sizeof(bk->name));
    strncpy(bk->author, author, sizeof(bk->author));
    bk->borrow = 0;

    pthread_spin_lock(&booksLock);
    cds_list_add_rcu(&(bk->node), &books);
    pthread_spin_unlock(&booksLock);
}

static void print_book(int id) {
    struct book *bk;

    rcu_read_lock();
    cds_list_for_each_entry_rcu(bk, &books, node) {
        if (bk->id == id) {
            printf("id : %d, name : %s, author : %s, borrow : %d, addr : %lx\n",
            bk->id, bk->name, bk->author, bk->borrow, (unsigned long)bk);
            
            rcu_read_unlock();
            return;
        }
    }

    rcu_read_unlock();
    printf("not exist book\n");
}

static int is_borrowed_book(int id) {
    struct book *bk;

    rcu_read_lock();
    cds_list_for_each_entry_rcu(bk, &books, node) {
        if (bk->id == id) {
            rcu_read_unlock();
            return bk->borrow;
        }
    }
    rcu_read_unlock();

    perror("not exist book\n");
    return -1;
}

static int borrow_book(int id) {
    struct book *bk = NULL;
    struct book *new_b = NULL;
    struct book *old_b =  NULL;

    rcu_read_lock();

    cds_list_for_each_entry(bk, &books, node) {
        if (bk->id == id) {
            if (bk->borrow) {
                rcu_read_unlock();
                return -1;
            }

            old_b = bk;
            break;
        }
    }

    if (old_b == NULL) {
        rcu_read_unlock();
        return -1;
    }
    
    new_b = malloc(sizeof(struct book));
    if (!new_b) {
        rcu_read_unlock();
        return -1;
    }

    memcpy(new_b, old_b, sizeof(struct book));
    new_b->borrow = 1;

    pthread_spin_lock(&booksLock);
    cds_list_replace_rcu(&old_b->node, &new_b->node);
    pthread_spin_unlock(&booksLock);

    rcu_read_unlock();

    synchronize_rcu();
    free(old_b);

    printf("borrow success %d, preempt_count : %d\n", id, preempt_count());
	return 0;
}

static int return_book(int id) {
    struct book *bk = NULL;
    struct book *new_bk = NULL;
    struct book *old_bk = NULL;

    rcu_read_lock();
    cds_list_for_each_entry(bk, &books, node) {
        if (bk->id == id) {
            if (bk->borrow == 0) {
                rcu_read_unlock();
                return -1;
            }

            old_bk = bk;
            break;
        }
    }

    if (old_bk == NULL) {
        rcu_read_unlock();
        return -1;
    }

    new_bk = malloc(sizeof(struct book));
    if (new_bk == NULL) {
        rcu_read_unlock();
        return -1;
    }


    memcpy(new_bk, old_bk, sizeof(struct book));
    new_bk->borrow = 0;

    pthread_spin_lock(&booksLock);
    cds_list_replace_rcu(&old_bk->node, &new_bk->node);
    pthread_spin_unlock(&booksLock);

    rcu_read_unlock();

    synchronize_rcu();
    free(old_bk);

    printf("return success %d, preempt_count : %d\n", id, preempt_count());
	return 0;
}

static void test_example(void) {
    add_book(0, "book1", "jb");
    add_book(1, "book2", "jb");

    print_book(0);
    print_book(1);

    printf("book1 borrow : %d\n", is_borrowed_book(0));
    printf("book2 borrow : %d\n", is_borrowed_book(1));

    borrow_book(0);
    borrow_book(1);

    print_book(0);
    print_book(1);

    return_book(0);
    return_book(1);

    print_book(0);
    print_book(1);
}



int main() {
    pthread_spin_init(&booksLock, PTHREAD_PROCESS_PRIVATE);
    rcu_init();
    test_example();
    return 0;
}
