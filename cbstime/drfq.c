/* cs194-24 Lab 2 */

#include "drfq.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>


enum drfq_state
{
    DQS_FREE,
    DQS_RUN,
    DQS_COMMIT,
};

struct drfq
{
	/* the mode, single or all */
    enum drfq_mode mode;
    /* number of tokens basically */
    int max_entry;
    /* number of threads that can run */
    size_t max_work_units;

    /* Signifies if this queue is valid
     * its set to 1 when it gets created
     * and then to 0 when request returns -1 to everyone
     * that way the next time create is called, it knows to create it
	 */
	int valid;

    /* Used to lock this drfq */
    int qlock;
    pthread_t owner;

    /* Array of tokens, each contains locks for the different threads */
    int num_tokens;
    struct drf_token* tokens;

    /* Used by ALL mode */
    int work_token;
};

/* The reason why I have this array of tokens which then hold locks
 * is so we don't have to do awful math to get the right lock
 */
struct drf_token{
	int uncomplete_locks;
	int num_locks;
	struct drf_lock* locks;
};

struct drf_lock{
	int state_lock;
	enum drfq_state state;
	pthread_t owner;
};

int drfq_init(drfq_t *queue)
{
    struct drfq *q;

    q = malloc(sizeof(*q));
    if (q == NULL)
		return -1;

    q->mode = DRFQ_MODE_INIT;
    q->max_entry = 0;
    q->tokens = NULL;

    *queue = q;
    return 0;
}

int drfq_create(drfq_t *queue, drf_t *drf,
		int max_entry, enum drfq_mode mode)
{
    struct drfq *q;
    q = *queue;
    int i;
    int j;
    struct drf_lock *lock;
    struct drf_token *token;

    // couldn't think of a better way... :/
    while (true){ 
    	//the while loop will stop when we get the lock
	    //lets try locking the queue first before we do anything
		if(__sync_bool_compare_and_swap(&(q->qlock), 0, 1)){
			//nobody has the lock, and we just locked it
			q->owner = pthread_self();
			if (q->valid == 0){
				//queue hasn't been created yet
			    q->mode = mode;
			    q->max_entry = max_entry;
			    q->max_work_units = drf_max_work_units(drf);
			    q->num_tokens = max_entry;
			    q->tokens = malloc(sizeof(struct drf_token) * max_entry);
			    q->work_token = 0;

			    switch (mode)
			    {
				    case DRFQ_MODE_SINGLE:
				    	//Theres only one lock per token since only one thread
				    	//needs to commit this lock for it to work
				    	for (i = 0; i < q->num_tokens; i++){
				    		//create a single drf_lock to populate the token
				    		lock = malloc(sizeof(struct drf_lock));
				    		lock->state_lock = 0;
				    		lock->state = DQS_FREE;
				    		lock->owner = -1;
				    		
				    		token = &(q->tokens[i]);
				    		token->num_locks = 1;
				    		token->locks = lock;
				    	}
						break;
				    case DRFQ_MODE_ALL:
				    	//There's max_entry locks per token since each thread
				    	//has to finish doing stuff to this token
				    	for (i = 0; i < q->num_tokens; i++){
				    		//lock is now an array of max_entry drf_lock
				    		lock = malloc(sizeof(struct drf_lock) * max_entry);

				    		token = &(q->tokens[i]);
				    		token->num_locks = q->max_work_units;
				    		token->locks = lock;

				    		//we should set the state of each of the locks
				    		for(j = 0; j < token->num_locks; j++){
				    			//we'll just reuse the pointer
				    			lock = &(token->locks[j]);
				    			lock->state_lock = 0;
				    			lock->state = DQS_FREE;
				    			lock->owner = -1;
				    		}
				    		//so now all the locks in the token are set
				    	}
						break;
				    case DRFQ_MODE_INIT:
				    	//bad news, you can't create a lock with INIT
						abort();
						break;
			    }

			    //the queue is now valid
				q->valid = 1; 
			} else {
				//queue has already been created
				//don't do anything, just quietly release the lock
				//leaving the else here for the awesome comments
			}

			//set the owner to -1 for consistency sake
			q->owner = -1;

			//free the lock
			q->qlock = 0;

			//break out of the loop by returning
			return 0;
		} else {
			//its locked, we should check if the thread is still alive
			if (pthread_kill(q->owner, 0) != 0){
				//the thread isn't running anymore
				//we should try unlocking it and start over
				__sync_bool_compare_and_swap (&(q->qlock), 1, 0);
				//if it fails, it means someone else unlocked it already
			} else {
				//the thread is still running and has the lock
				//we should do the loop again just in case that thread dies
			}
		}
    }
    return 0;
}

int drfq_request(drfq_t *queue)
{
    struct drfq *q;
    struct drf_token *token;
    struct drf_lock *lock;
    struct drf_lock *locks;
    int i;
    int j;
    int k;
    q = *queue;
    //we only need to grab locks on locks >_<
    //...you get the idea

	//depending on the mode, we need to do different stuff
	switch(q->mode) {
		case DRFQ_MODE_SINGLE:
			//find a token with uncomplete locks
			for(i = 0; i < q->num_tokens; i++){
				token = &(q->tokens[i]);
				//there should only be one lock
				lock = token->locks;

				//lets try grabbing the lock
				if (__sync_bool_compare_and_swap(&(lock->state_lock), 0, 1)){
					//we got the lock! lets check its state.
					//DQS_FREE, we take it and return
					//DQS_RUN, check the thread 
					//DQS_COMMIT, keep going...
					//remember that i is the token number....
					switch(lock->state){
						case DQS_FREE:
							lock->state = DQS_RUN;
							lock->owner = pthread_self();
							lock->state_lock = 0;
							return i;
						case DQS_RUN:
							//the thread might be dead, lets make sure....
							if (pthread_kill(lock->owner, 0) != 0){
								//thread is dead, we should just take it
								lock->owner = pthread_self();
								lock->state_lock = 0;
								return i;
							} else {
								//thread is still alive, we should just continue
							}
							break;
						case DQS_COMMIT:
							//this token is done, we should just keep going
							break;
					}
				} else {
					//couldn't grab the lock but the thread might be dead...
					if (pthread_kill(lock->owner, 0) != 0){
						//thread is dead, unlock it, start over
						__sync_bool_compare_and_swap(&(lock->state_lock), 1, 0);
						//we should try unlocking it and try again from the beginning
						//by setting i = 0
						i = 0;
					}

				}
			}

			break; //break for SINGLE case
		case DRFQ_MODE_ALL:
			while (true){
				if (q->work_token == q->num_tokens){
					return -1;
				}

				i = 0; //used if this thread saw itself already

				//lock is an array of locks
				token = &(q->tokens[q->work_token]); 
				locks = token->locks;
				for(j = 0; j < token->num_locks; j++){
					lock = &(locks[j]);

					//check if we see ourself
					if (lock->owner == pthread_self()){
						i = 1;
						break;//breaks out of for loop
					}

					//we haven't seen ourself and this lock 
					//isn't finished
					if (i != 1 && lock->state != DQS_COMMIT){
						//check the rest of the tokens to make sure
						//we're not there either
						for (k = j; k < token->num_locks; k++){
							if (lock->owner == pthread_self()){
								//we see ourself in the future
								i = 1;
								break; //break out of for
							}
						}

						//check to see if we saw ourself in the future
						if (i != 1){
							//we didn't see ourself in the future,
							//we still need to do work!
							//lets try grabbing the lock
							if (__sync_bool_compare_and_swap(&(lock->state_lock), 0, 1)){
								//we got the lock! lets check its state.
								//DQS_FREE, we take it and return
								//DQS_RUN, check the thread 
								//DQS_COMMIT, keep going...
								//remember that i is the token number....
								switch(lock->state){
									case DQS_FREE:
										lock->state = DQS_RUN;
										lock->owner = pthread_self();
										lock->state_lock = 0;
										return q->work_token;
									case DQS_RUN:
										//the thread might be dead, lets make sure....
										if (pthread_kill(lock->owner, 0) != 0){
											//thread is dead, we should just take it
											lock->owner = pthread_self();
											lock->state_lock = 0;
											return q->work_token;
										} else {
											//thread is still alive, we should just continue
										}
										break;
									case DQS_COMMIT:
										//this token is done, we should just keep going
										break;
								}
							} else {
								//couldn't grab the lock but the thread might be dead...
								if (pthread_kill(lock->owner, 0) != 0){
									//thread is dead, unlock it, start over
									__sync_bool_compare_and_swap(&(lock->state_lock), 1, 0);
									//we should try unlocking it and try again from the beginning
									//by setting i = 0
								}

							}//end of try grabbing lock if
						} else {
							//we saw ourself, break out of for loop
							break;
						}//end of we haven't seen ourselves if

					}//end of if I haven't seen myself and the lock isn't committed

				}//end of lock iteration

				//didn't see a single lock we should take
				//we should loop until we do or until everything is done
			}//end while
			break;
		case DRFQ_MODE_INIT:
			abort();
			break;

	}

	//didn't see a single lock we could take
	return -1;
}

//commit work
//subtract number of uncomplete locks
int drfq_commit(drfq_t *queue, int token_num)
{
    struct drfq *q;
    struct drf_token *token;
    struct drf_lock *lock;
    struct drf_lock *locks;
    int uncommitted;
    int i;
    q = *queue;

    switch (q->mode) {
	    case DRFQ_MODE_SINGLE:
	    	//get the lock
			token = &(q->tokens[token_num]);
			lock = token->locks;
			//keep trying to grab the lock
			while (!__sync_bool_compare_and_swap(&(lock->state_lock), 0, 1));
			lock->state = DQS_COMMIT;
			lock->state_lock = 0;
			break;
	    case DRFQ_MODE_ALL:
			token = &(q->tokens[token_num]);
			locks = token->locks;
			//go through all the locks to find yours
			for (i = 0; i < token->num_locks; i++){
				lock = &(locks[i]);
				if (lock->owner == pthread_self()){
					//keep trying to grab the lock
					while (!__sync_bool_compare_and_swap(&(lock->state_lock), 0, 1));
					lock->state = DQS_COMMIT;
					lock->state_lock = 0;
				}
				//find uncommitted locks
				if (lock->state != DQS_COMMIT){
					uncommitted++;
				}
			}

			//check uncommitted locks, it its 0, we need to increment the work_token
			//but you should check to make sure the work_token is still the same
			//as the token_num before incrementing it
			while(!__sync_bool_compare_and_swap(&(q->qlock), 0, 1));
			if (q->work_token == token_num){
				q->work_token++;
			}
			q->qlock = 0;
			break;
    	case DRFQ_MODE_INIT:
			abort();
			break;
    }

    return 0;
}
