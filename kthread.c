#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

/************
 * kt_for() *
 ************/

struct kt_for_t;

typedef struct {
    struct kt_for_t *t;
    long i;
} ktf_worker_t;

typedef struct kt_for_t {
    int n_threads;
    long n;
    ktf_worker_t *w;

    void (*func)(void *, long, int);

    void *data;
} kt_for_t;

static inline long steal_work(kt_for_t *t) {
    int i, min_i = -1;
    long k, min = LONG_MAX;
    for (i = 0; i < t->n_threads; ++i) {
        if (min > t->w[i].i) {
            min = t->w[i].i, min_i = i;
        }
    }
    k = __sync_fetch_and_add(&t->w[min_i].i, t->n_threads);
    return k >= t->n ? -1 : k;
}

static void *ktf_worker(void *data) {
    ktf_worker_t *w = (ktf_worker_t *)data;
    long i;
    for (;;) {
        i = __sync_fetch_and_add(&w->i, w->t->n_threads);
        if (i >= w->t->n) {
            break;
        }
        w->t->func(w->t->data, i, w - w->t->w);
    }
    while ((i = steal_work(w->t)) >= 0) {
        w->t->func(w->t->data, i, w - w->t->w);
    }
    pthread_exit(0);
}

void kt_for(int n_threads, void (*func)(void *, long, int), void *data, long n) {
    int i;
    kt_for_t t;
    pthread_t *tid;
    t.func = func, t.data = data, t.n_threads = n_threads, t.n = n;
    t.w = (ktf_worker_t *)alloca(n_threads * sizeof(ktf_worker_t));
    tid = (pthread_t *)alloca(n_threads * sizeof(pthread_t));
    for (i = 0; i < n_threads; ++i) {
        t.w[i].t = &t, t.w[i].i = i;
    }
    for (i = 0; i < n_threads; ++i) {
        pthread_create(&tid[i], 0, ktf_worker, &t.w[i]);
    }
    for (i = 0; i < n_threads; ++i) {
        pthread_join(tid[i], 0);
    }
}

/*****************
 * kt_pipeline() *
 *****************/

struct ktp_t;

typedef struct {
    struct ktp_t *pl;
    int64_t index; //工作线程的编号
    int step; //工作线程的步骤，可能是控制数据处理的先后顺序
    void *data; //线程中处理的数据
} ktp_worker_t;

//多线程处理数据的共享参数数据结构
typedef struct ktp_t {
    void *shared;

    void *(*func)(void *, int, void *); //工作线程执行的函数指针

    int64_t index;
    int n_workers, n_steps;
    ktp_worker_t *workers; //线程work指针
    pthread_mutex_t mutex; //线程中的互斥锁
    pthread_cond_t cv; //线程中的条件变量
} ktp_t;

//多线程执行的入口函数，该函数控制执行哪些业务逻辑
static void *ktp_worker(void *data) {
    ktp_worker_t *w = (ktp_worker_t *)data;
    ktp_t *p = w->pl;
    while (w->step < p->n_steps) {
        // test whether we can kick off the job with this worker
        pthread_mutex_lock(&p->mutex);
        for (;;) {
            int i;
            // test whether another worker is doing the same step
            for (i = 0; i < p->n_workers; ++i) {
                // ignore itself
                if (w == &p->workers[i]) {
                    continue;
                }
                if (p->workers[i].step <= w->step && p->workers[i].index < w->index) {
                    break;
                }
            }
            if (i == p->n_workers) {
                break;
            } // no workers with smaller indices are doing w->step or the previous steps
            pthread_cond_wait(&p->cv, &p->mutex);
        }
        pthread_mutex_unlock(&p->mutex);

        // working on w->step
        w->data = p->func(p->shared, w->step, w->step ? w->data : 0); // for the first step, input is NULL

        // update step and let other workers know
        pthread_mutex_lock(&p->mutex);
        w->step = w->step == p->n_steps - 1 || w->data ? (w->step + 1) % p->n_steps : p->n_steps;
        if (w->step == 0) {
            w->index = p->index++;
        }
        pthread_cond_broadcast(&p->cv);
        pthread_mutex_unlock(&p->mutex);
    }
    pthread_exit(0);
}

/**
 * 线程控制管理，启动工作线程
 * @param n_threads 线程数量
 * @param func 线程执行函数
 * @param shared_data 线程共享数据
 * @param n_steps 需要执行的步数
 */
void kt_pipeline(int n_threads, void *(*func)(void *, int, void *), void *shared_data, int n_steps) {
    ktp_t aux;
    int i;

    if (n_threads < 1) {
        n_threads = 1;
    }
    aux.n_workers = n_threads;
    aux.n_steps = n_steps;
    aux.func = func; //work function
    aux.shared = shared_data;
    aux.index = 0;
    pthread_mutex_init(&aux.mutex, 0);
    pthread_cond_init(&aux.cv, 0);

    //分配多线程，并初始化线程执行参数
    aux.workers = (ktp_worker_t *)alloca(n_threads * sizeof(ktp_worker_t));
    for (i = 0; i < n_threads; ++i) {
        ktp_worker_t *w = &aux.workers[i];
        w->step = 0;
        w->pl = &aux;
        w->data = 0;
        w->index = aux.index++;
    }

    //启动多线程执行比对任务
    pthread_t *tid = (pthread_t *)alloca(n_threads * sizeof(pthread_t));
    for (i = 0; i < n_threads; ++i) {
        pthread_create(&tid[i], 0, ktp_worker, &aux.workers[i]);
    }

    //通过join等待各线程执行结束后，销毁线程
    for (i = 0; i < n_threads; ++i) {
        pthread_join(tid[i], 0);
    }
    pthread_mutex_destroy(&aux.mutex);
    pthread_cond_destroy(&aux.cv);
}
