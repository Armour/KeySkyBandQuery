/*-------------------------------------------------
 *
 *  sky_query.h
 *  KeySkyBandQuery
 *
 *  Created by Armour on 7/10/15.
 *  Copyright (c) 2015 Armour. All rights reserved.
 *
 *-------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "sky_point_list.h"
#include "sky_bucket_list.h"
#include "sky_hashtable.h"

int sky_k;                  /* The k value that in skyband query */
int sky_dim;                /* The dimension of point in skyband query */
int sky_cnt;                /* The number of the points in skyband query */

double **tmp_pointer;

int s_size;                     /* Point number in S */
int stwh_size;                  /* Point number in Stwh */
int ses_size;                   /* Point number in Ses */
int sg_size;                    /* Point number in Sg */
int tmp_size;

SkyPoint *s_head, *s_tail;             /* Head/tail point for S */
SkyPoint *stwh_head, *stwh_tail;       /* Head/tail point for Stwh */
SkyPoint *ses_head, *ses_tail;         /* Head/tail point for Ses */
SkyPoint *sg_head, *sg_tail;           /* Head/tail point for Sg */
SkyPoint *tmp_head, *tmp_tail;

SkyBucket *first_bucket, *last_bucket;         /* First and last bucket for all bitmap that already knows */
SkyBucket *tmp_bucket;

HashTable *h;                           /* Hashtable for skyband query */
ListNode *tmp_listnode;

FILE *fin, *fout;                       /* Used for testing */

/*
 * Function: InputPoints
 * -------------------
 *   Input all points (include data and bitmap)
 *
 *   sky_cnt: The k value that in skyband query
 *   sky_dim: The dimension of point in skyband query
 *
 *   returns: void
 */

void InputPoints() {
    int i, j;
    int bitmap;

    s_head = StartPoint(&s_size, &s_head, &s_tail, sky_dim);            /* Create head point of S */

    tmp_pointer = (double **)malloc(sizeof(double*) * sky_cnt);

    for (i = 0; i < sky_cnt; i++) {
        tmp_head = StartPoint(&tmp_size, &tmp_head, &tmp_tail, sky_dim);        /* Create temp input point */
        tmp_pointer[i] = (double *)malloc(sizeof(double) * sky_dim);            /* Malloc memory to input point's data */
        tmp_head->data = &(tmp_pointer[i]);
        tmp_head->bitmap = (char *)malloc(sizeof(char) * sky_dim);              /* Malloc memory to input point's bimap */

        for(j = 0; j < sky_dim; j++)
            fscanf(fin, "%lf", (*(tmp_head->data) + j));                /* Input point's data from file */

        for(j = 0; j < sky_dim; j++) {                                  /* Input point's bitmap from file */
            fscanf(fin, "%d", &bitmap);
            if (bitmap != 1)
                *(tmp_head->bitmap + j) = '0';
            else
                *(tmp_head->bitmap + j) = '1';
        }

        PushPoint(tmp_head, &s_size, &s_tail);                          /* Push all points to S */
    }
}

/*
 * Function: IsP1DominateP2
 * -------------------
 *   To judge weather point p2 is dominated by point p1
 *
 *   p1: The point used to compare with p2
 *   p2: The point used to compare with p1
 *
 *   returns:
 *      if p1 dominate p2 return 1
 *                   else return 0
 */

int IsP1DominateP2(SkyPoint *p1, SkyPoint *p2) {
    double x1, x2;
    int i;
    int dim = p1->dim;
    int is_small = 0;                       /* Used to decide if there exist one dimension that p1 is smaller than p2 */
    int cnt_small_or_equal = 0;             /* Count of dimension number that p1 is smaller than or equal to p2 in such dimension */
    int is_null_x1, is_null_x2;
    if (!p1 || !p2) return 0;               /* If p1 or p2 is not exist */
    for (i = 0; i < dim; i++) {             /* Look at each dimension */
        x1 = *(*(p1->data)+i);
        x2 = *(*(p2->data)+i);
        is_null_x1 = (*(p1->bitmap + i)) == '0';
        is_null_x2 = (*(p2->bitmap + i)) == '0';
        if (is_null_x1 || is_null_x2) {         /* If p1 or p2's bitmap is '0', which means incomplete data */
            cnt_small_or_equal++;
        } else {                            /* Else just do comparation */
            if (x1 <= x2) cnt_small_or_equal++;
            if (x1 < x2) is_small = 1;
        }
    }
    if ((cnt_small_or_equal == dim) && is_small)        /* All dimension not larger than p2 and exist one dimension is smaller than p2 */
        return 1;
    else
        return 0;
}

/*
 * Function: CmpFunc
 * -------------------
 *   Compare function that used in QsortStwh function
 *
 *   p1: The point used to compare with p2
 *   p2: The point used to compare with p1
 *
 *   returns:
 *      if p1's cnt_domi is smaller than p2's, return > 0
 *      if p2's cnt_domi is smaller than p1's, return < 0
 *                                        else return = 0
 */

int CmpFunc(const void *p1, const void *p2) {
    const SkyPoint **t1 = (const SkyPoint **)p1;
    const SkyPoint **t2 = (const SkyPoint **)p2;
    return (**t2).cnt_domi - (**t1).cnt_domi;
}

/*
 * Function: QsortStwh
 * -------------------
 *   Qsort on Stwh list, first we make a copy of list as an array,
 *   after we finished the qsort on array, we'll put it back to list
 *
 *   n: The size of Stwh list (array)
 *
 *   returns: void
 */

void QsortStwh(int n) {
    int i;
    SkyPoint *point_array[n];                       /* Dynamically alloc memory for an array with size n */
    SkyPoint *tmp_point;
    if (sky_k > 1) {                                /* Only need qsort when sky_k is larger than 1 */
        tmp_point = stwh_head;
        for (i = 0; i < n; i++) {                   /* Build the array */
            point_array[i] = tmp_point->next;
            tmp_point = tmp_point->next;
        }
        qsort(point_array, n, sizeof(point_array[0]), CmpFunc);         /* Qsort this array */
        stwh_head->next = point_array[0];           /* Put first point in array back to list */
        point_array[0]->prev = stwh_head;
        point_array[0]->next = NULL;
        stwh_tail = point_array[0];
        for (i = 1; i < n; i++) {                   /* Put rest points in array back to list */
            stwh_tail->next = point_array[i];
            point_array[i]->prev = stwh_tail;
            point_array[i]->next = NULL;
            stwh_tail = point_array[i];
        }
    }
}

/*
 * Function: ThicknessWarehouse
 * -------------------
 *
 *
 *   returns: void
 */

void ThicknessWarehouse() {
    int i, j;
    int cnt_a, cnt_b;

    SkyPoint *iter_a;
    SkyPoint *iter_b;
    SkyPoint *tmp_point = NULL;
    SkyPoint *tmp_point2 = NULL;
    SkyPoint *tmp_next;
    SkyPoint **tmp_array;

    /* Create start point of Stwh, Ses and Sg */
    stwh_head = StartPoint(&stwh_size, &stwh_head, &stwh_tail, sky_dim);
    ses_head = StartPoint(&ses_size, &ses_head, &ses_tail, sky_dim);
    sg_head = StartPoint(&sg_size, &sg_head, &sg_tail, sky_dim);

    //////////////////////////////////////////////////////////////////////////
    //                                                                      //
    // [STEP 1]                                                             //
    //      Push all points in S to every bucket according to bitmap        //
    //                                                                      //
    //////////////////////////////////////////////////////////////////////////

    /* Create hashtable */
    h = InitTable(sky_cnt);

    /* Push every point in S to a bucket depends on its bitmap */
    first_bucket = NULL;
    tmp_point = s_head;
    tmp_next = tmp_point->next;
    while (tmp_next != NULL) {
        tmp_point = tmp_next;
        tmp_next = tmp_point->next;
        tmp_listnode = Find(tmp_point->bitmap, h, sky_dim);             /* Find the list of nodes in hashtable according to bimap */
        if (tmp_listnode == NULL) {
            tmp_bucket = (SkyBucket *)malloc(sizeof(SkyBucket));        /* If not exist, then we create a node for this bitmap in hashtable */
            InitBucket(tmp_bucket, sky_dim);
            Insert(tmp_point->bitmap, h, sky_dim, tmp_bucket, &first_bucket, &last_bucket);
        } else {
            tmp_bucket = tmp_listnode->bucket;                          /* Get the bucket of this bitmap */
        }
        PushPoint(tmp_point, &tmp_bucket->data_size, &tmp_bucket->data_tail);       /* Push point into the bucket of this bitmap */
    }

    //////////////////////////////////////////////////////////////////////////
    //                                                                      //
    // [STEP 2]                                                             //
    //      Divide points in every bucket into Sl and Sln, then put all     //
    //  points in Sl into Stwh.                                             //
    //                                                                      //
    //////////////////////////////////////////////////////////////////////////

    tmp_bucket = first_bucket;
    tmp_array = (SkyPoint **)malloc(sizeof(SkyPoint*) * tmp_bucket->data_size);
    while (tmp_bucket != NULL) {
        tmp_point = tmp_bucket->data_head;
        tmp_array[0] = tmp_point;
        for (i = 0; i < tmp_bucket->data_size; i++) {                           /* Put points in list into array */
            tmp_point = tmp_point->next;
            tmp_array[i] = tmp_point;
        }
        for (i = 0; i < tmp_bucket->data_size; i++) {
            tmp_point = tmp_array[i];
            for (j = i + 1; j < tmp_bucket->data_size; j++) {                       /* Compare each pair of points in array */
                tmp_point2 = tmp_array[j];
                if (IsP1DominateP2(tmp_point2, tmp_point)) {                /* If point A dominate point B */
                    tmp_point->cnt_domi++;                                  /* Add cnt_domi of B */
                    if (tmp_point->cnt_domi>= sky_k) {                      /* If cnt_domi of B is larger than sky_k, we put B into Sln */
                        PushPoint(tmp_point, &tmp_bucket->sln_size, &tmp_bucket->sln_tail);
                        break;
                    }
                }
            }
            if (j == tmp_bucket->data_size)             /* which means data[j] is not dominted more than k times, then put it into Sl */
                PushPoint(tmp_point, &stwh_size, &stwh_tail);
        }
        tmp_bucket = tmp_bucket->next;
    }

    /* Free the memory alloc of tmp_array */
    free(tmp_array);

    //////////////////////////////////////////////////////////////////////////
    //                                                                      //
    // [STEP 3]                                                             //
    //      Quick sort all points in Stwh according to cnt_domi             //
    //                                                                      //
    //////////////////////////////////////////////////////////////////////////

    QsortStwh(stwh_size);

    //////////////////////////////////////////////////////////////////////////
    //                                                                      //
    // [STEP 4]                                                             //
    //      Comparing all points in Swth and push expired points to Ses     //
    //                                                                      //
    //////////////////////////////////////////////////////////////////////////

    cnt_a = 0;
    iter_a = stwh_head->next;
    while (iter_a != NULL) {                                                        /* Iter_a starts from stwh_head to stwh_tail */
        cnt_a++;
        tmp_next = iter_a->next;
        iter_b = stwh_tail;
        cnt_b = 0;
        while (iter_b != stwh_head) {                                               /* Iter_b starts from stwh_tali to stwh_head */
            cnt_b++;
            tmp_point = iter_b->prev;
            if (SameBitmap(iter_a->bitmap, iter_b->bitmap, sky_dim))                /* If converge at same bitmap, then break */
                break;
            if (IsP1DominateP2(iter_b, iter_a)) {
                iter_a->cnt_domi++;
                if (iter_a->cnt_domi >= sky_k) {
                    DeletePoint(cnt_a, &stwh_size, &stwh_head, &stwh_tail);
                    PushPoint(iter_a, &ses_size, &ses_tail);
                    cnt_a--;
                    break;
                }
            }
            if (IsP1DominateP2(iter_a, iter_b)) {
                iter_b->cnt_domi++;
                if (iter_b->cnt_domi >= sky_k) {
                    if (tmp_next == iter_b)                 /* If two nearby nodes, we delete the second, then update first node's next */
                        tmp_next = iter_b->next;
                    DeletePoint(stwh_size - cnt_b + 1, &stwh_size, &stwh_head, &stwh_tail);
                    PushPoint(iter_b, &ses_size, &ses_tail);
                    cnt_b--;
                }
            }
            iter_b = tmp_point;
        }
        iter_a = tmp_next;
    }

    //////////////////////////////////////////////////////////////////////////
    //                                                                      //
    // [STEP 5]                                                             //
    //      Comparing points in Swth with Ses and Sln, get the final        //
    //  result Sg.                                                          //
    //                                                                      //
    //////////////////////////////////////////////////////////////////////////

    /* Stwh VS Ses */
    cnt_a = 0;
    iter_a = stwh_head->next;
    while (iter_a != NULL) {
        cnt_a++;
        tmp_next = iter_a->next;
        iter_b = ses_head->next;
        while (iter_b != NULL) {
            if (IsP1DominateP2(iter_b, iter_a)) {
                iter_a->cnt_domi++;
                if (iter_a->cnt_domi >= sky_k) {
                    DeletePoint(cnt_a, &stwh_size, &stwh_head, &stwh_tail);
                    cnt_a--;
                    break;
                }
            }
            iter_b = iter_b->next;
        }
        iter_a = tmp_next;
    }

    /* Stwh VS Sln */
    cnt_a = 0;
    iter_a = stwh_head->next;
    while (iter_a != NULL) {
        cnt_a++;
        tmp_next = iter_a->next;
        tmp_bucket = first_bucket;
        while (tmp_bucket != NULL) {
            iter_b = tmp_bucket->sln_head->next;
            while (iter_b != NULL) {
                if (IsP1DominateP2(iter_b, iter_a)) {
                    iter_a->cnt_domi++;
                    if (iter_a->cnt_domi >= sky_k) {
                        DeletePoint(cnt_a, &stwh_size, &stwh_head, &stwh_tail);
                        cnt_a--;
                        break;
                    }
                }
                iter_b = iter_b->next;
            }
            if (iter_b != NULL) break;
            tmp_bucket = tmp_bucket->next;
        }
        iter_a = tmp_next;
    }

    /* Sg is equal to Stwh now, and it is the answer of the query */
    sg_size = stwh_size;
    sg_head = stwh_head;
    sg_tail = stwh_tail;
    sg_head = stwh_head;
}

/*
 * Function: Init
 * -------------------
 *   Initialize varibles & input test data
 *
 *   sky_cnt: The k value that in skyband query
 *   sky_dim: The dimension of point in skyband query
 *   sky_k: The number of the points in skyband query
 *
 *   returns: void
 */

void Init() {
    tmp_size = 0;
    s_size = 0;
    stwh_size = 0;
    ses_size = 0;
    sg_size = 0;

    first_bucket = NULL;
    last_bucket = NULL;

    fin = fopen("/Users/armour/Desktop/KSkyBandQuery/KSkyBandQuery-C/KSkyBandQuery-C/Test/stdin.txt", "r+");
    fscanf(fin, "%d %d %d", &sky_cnt, &sky_dim, &sky_k);

    InputPoints();                      /* Input all points */

    fclose(fin);
}

/*
 * Function: Query
 * -------------------
 *   Initialize clock & Run the thickness-warehouse algorithm & Calculate time consumption
 *
 *   start: Start time of thickness-warehouse algorithm
 *   diff: Time duration of thickness-warehouse algorithm
 *
 *   returns: void
 */

void Query() {
    clock_t start = clock(), diff;

    ThicknessWarehouse();

    diff = clock() - start;
    unsigned long msec = diff * 1000 / CLOCKS_PER_SEC;          /* Convert to second and  millisecond */
    printf("Skyline: %lus %lums\n", msec/1000, msec%1000);
}

/*
 * Function: Output
 * -------------------
 *   Output the query result
 *
 *   sg_head: Start point of Sg, which is the final result list
 *
 *   returns: void
 */

void Output() {
    int i, j;
    fout = fopen("/Users/armour/Desktop/KSkyBandQuery/KSkyBandQuery-C/KSkyBandQuery-C/Test/skylineout.txt", "w+");
    tmp_head = sg_head;
    for (i = 0; i < sg_size; i++) {
        tmp_head = tmp_head->next;
        for (j = 0; j < sky_dim; j++)
            fprintf(fout, "%.6lf ", *(*(tmp_head->data) + j));          /* default printing in double type */
        fprintf(fout, "\n");
    }
    fclose(fout);
}

/* Main function */

int main() {
    Init();
    Query();
    Output();
}
